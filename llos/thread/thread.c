///////////////////////////////////////////////////////////////
// File Name: thread.c
// Author: tt
// Email: tttt@xiyoulinux.org
// Created Time : 2020-10-10 19:08:11
// Description:
///////////////////////////////////////////////////////////////
#include "thread.h"
#include "stdint.h"
#include "string.h"
#include "../kernel/global.h"
#include "memory.h"
#include "../kernel/debug.h"
#include "../kernel/interrupt.h"
#include "../lib/kernel/print.h"
#include "../lib/kernel/list.h"
#include "../userprog/process.h"
#include "sync.h"
#include "../fs/file.h"

#define PG_SIZE 4096 

struct lock pid_lock;
struct task_struct* main_thread;                    //主线程PCB 
struct list thread_ready_list;                      //就绪队列,就绪队列中的线程都是可用于直接上处理器运行的，阻塞的线程是不放在就绪队列中的
struct list thread_all_list;                        //所有任务队列,全部线程队列 
static struct list_elem* thread_tag;                //用于保存队列中的线程结点　
struct task_struct* idle_thread;                    //idle 线程

extern void switch_to(struct task_struct* cur, struct task_struct* next);
extern void init(void);


//系统空闲时运行的线程
static void idle(void* arg UNUSED) {

    while(1) {

        thread_block(TASK_BLOCKED);
        //执行hlt时必须要保证目前处在开中断的状态下
        
        asm volatile ("sti;hlt" : : : "memory");
    }
}


//获取当前线程PCB指针
struct task_struct* running_thread() {

    uint32_t esp;
    asm("mov %%esp, %0" : "=g"(esp));
    //取esp整数部分，即PCB起始地址 
    //各个线程所用的0级栈都是在自己的PCB当中，因此取当前栈指针的高20位作为当前运行线程PCB。
    return (struct task_struct*)(esp & 0xfffff000);
}

//分配pid 
static pid_t allocate_pid(void) {

    static pid_t next_pid = 0;              //全局静态变量
    lock_acquire(&pid_lock);
    next_pid++;
    lock_release(&pid_lock);

    return next_pid;
}



//由kernel_thread 去执行 function(func_arg) 
static void kernel_thread(thread_func* function, void* func_arg) {

    //执行function 前要开中断，避免后面的时钟中断被屏蔽，而无法调度其他线程 
    intr_enable();                                  //开中断
    function(func_arg);
}


/*fork 进程时为其分配pid, 因为allocate_pid已经是静态了,别的文件无法调用, 不想改变海曙定义, 所以定义了
 * fork_pid函数来封装一下*/
pid_t fork_pid(void) {

    return allocate_pid();
}


//初始化 线程栈 thread_stack 
//将待执行的函数和参数放到thread_stack 中相应的位置 
void thread_create(struct task_struct* pthread, thread_func function, void* func_arg) {

    //先预留中断使用栈的空间，可见 thread.h 
    pthread->self_kstack -= sizeof(struct intr_stack);

    //在留出线程栈空间 
    pthread->self_kstack -= sizeof(struct thread_stack);
    
    struct thread_stack* kthread_stack = (struct thread_stack*)pthread->self_kstack;
    
    //位于栈顶 
    kthread_stack->eip = kernel_thread; 
    kthread_stack->function = function;
    kthread_stack->func_arg = func_arg;
    kthread_stack->ebp = kthread_stack->ebx = kthread_stack->esi = kthread_stack->edi = 0;
}


//初始化线程基本信息
void init_thread(struct task_struct* pthread, char* name, int prio) {
    
    //将所在的页清零 
    memset(pthread, 0 ,sizeof(*pthread));
    pthread->pid = allocate_pid();              //获取pid 
    strcpy(pthread->name, name); 

    if( pthread == main_thread ) {
        //由于把main函数也封装成一个线程，并且它一直是运行的，故将其直接设为 TASK_RUNNING  
        pthread->status = TASK_RUNNING;
    }else {

        pthread->status = TASK_READY;
    }
    
    //self_kstack 是线程自己在内核态下使用的栈顶地址

    //self_kstack 是线程自己在内核态下那个栈顶地址 是线程自己在0特权级下所用的栈。在线程创建之初，它被初始化为线程PCB的最顶端
    pthread->self_kstack = (uint32_t*)((uint32_t)pthread + PG_SIZE);
    pthread->priority = prio;
    pthread->ticks = prio;
    pthread->elapsed_ticks = 0;                                     //表示线程尚未执行过
    pthread->pgdir = NULL;

    /*预留标准输入输出*/
    pthread->fd_table[0] = 0;                                       //标准输入
    pthread->fd_table[1] = 1;                                       //标准输出
    pthread->fd_table[2] = 2;                                       //标准错误
    /*其余位置全置为 -1*/
    uint8_t fd_idx = 3;
    while(fd_idx < MAX_FILES_OPEN_PER_PROC) {
        
        pthread->fd_table[fd_idx] = -1;
        fd_idx++;
    }

    pthread->cwd_inode_nr = 0;                                      //以根目录作为默认工作路径
    pthread->parent_pid = -1;                                       //-1表示没有父进程
    pthread->stack_magic = 0x19870916;                              //自定义的魔数

}

//创建一优先级为prio的线程,线程名为 name ,线程所执行的函数是(function(func_arg))
struct task_struct* thread_start(char* name, int prio, thread_func function, void* func_arg) {

    //pcb 都位于内核空间，包括用户进程的pcb也是在内核空间
    //在内核申请一页内存,返回的是页的起始地址,所以thread　指向的是PCB的最低地址
    struct task_struct* thread = get_kernel_pages(1);
    //　将３个参数写入线程的PCB，并且完成PCB一级的其他初始化;
    init_thread(thread, name, prio);
    
    //给thread 赋初值
    thread_create(thread, function, func_arg);

    //确保之前不在就绪队列中
    ASSERT( !elem_find(&thread_ready_list, &thread->general_tag) );
    //加入就绪队列
    list_append(&thread_ready_list, &thread->general_tag);

    //确保之前不在全部队列
    ASSERT( !elem_find(&thread_all_list, &thread->all_list_tag) );
    //加入全部线程队列
    list_append(&thread_all_list, &thread->all_list_tag);

    return thread;
}

//将kernel中的main函数完善为主线程 
static void make_main_thread(void) {

    //因为main线程早已运行,在loader.S 中进入内核时的 mov esp, 0xc009f000,就是为其预留pcb的，因此pcb地址为0xc009e000 
    //不需要通过 get_kernel_pages 另分配一页 
    main_thread = running_thread();
    init_thread(main_thread, "main", 31);

    //main　函数是当前线程，当前线程不在 thread_ready_list 中
    //所以只将其加在 thread_all_list 中
    ASSERT( !elem_find(&thread_all_list, &main_thread->all_list_tag) ); 
    list_append(&thread_all_list, &main_thread->all_list_tag);
}

/*以填充空格的方式输出 buf*/
static void pad_print(char* buf, int32_t buf_len, void* ptr, char format) {

    memset(buf, 0, buf_len);
    uint8_t out_pad_0idx = 0;

    switch(format) {

    case 's':

        out_pad_0idx = sprintf(buf, "%s", ptr);
        break;
    case 'd':

        out_pad_0idx = sprintf(buf, "%d", *((int16_t*)ptr));
    case 'x':

        out_pad_0idx = sprintf(buf, "%x", *((uint32_t *)ptr));
    }

    while(out_pad_0idx < buf_len) {

        //以空格填充
        buf[out_pad_0idx] = '0';
        out_pad_0idx++;
    }

    sys_write(stdout_no, buf, buf_len - 1);
}

/* 用于在list_traversal函数中的回调函数,用于针对线程队列的处理 */
static bool elem2thread_info(struct list_elem* pelem, int arg UNUSED) {

    struct task_struct* pthread = elem2entry(struct task_struct, all_list_tag, pelem);
    char out_pad[16] = {0};

    pad_print(out_pad, 16, &pthread->pid, 'd');

    if (pthread->parent_pid == -1) {
 
        pad_print(out_pad, 16, (void *)"NULL", 's');
    } else { 
        
        pad_print(out_pad, 16, &pthread->parent_pid, 'd');
    }

    switch (pthread->status) {
    case 0:
       
        pad_print(out_pad, 16, (void *)"RUNNING", 's');
        break;
    case 1:
        
        pad_print(out_pad, 16, (void *)"READY", 's');
        break;
    case 2:
        
        pad_print(out_pad, 16, (void *)"BLOCKED", 's');
        break;
    case 3:
        
        pad_print(out_pad, 16, (void *)"WAITING", 's');
        break;
    case 4:
        
        pad_print(out_pad, 16, (void *)"HANGING", 's');
        break;
    case 5:
        
        pad_print(out_pad, 16, (void *)"DIED", 's');
    }
    pad_print(out_pad, 16, &pthread->elapsed_ticks, 'x');

    memset(out_pad, 0, 16);
    ASSERT(strlen(pthread->name) < 17);
    memcpy(out_pad, pthread->name, strlen(pthread->name));
    strcat(out_pad, "\n");
    
    sys_write(stdout_no, out_pad, strlen(out_pad));
    
    return false;	// 此处返回false是为了迎合主调函数list_traversal,只有回调函数返回false时才会继续调用此函数
}


/*打印任务列表*/
void sys_ps(void) {

    char* ps_title = "PID       PPID        STAT        TICKS       COMMAND\n";
    sys_write(stdout_no, ps_title, strlen(ps_title));
    list_traversal(&thread_all_list, elem2thread_info, 0);
}

//实现任务调度
void schedule() {

    ASSERT(intr_get_status() == INTR_OFF);

    struct task_struct* cur = running_thread();
    if (cur->status == TASK_RUNNING) {

        //若此线程只是CPU时间片到了，将其加入到就绪队列队尾
        ASSERT( !elem_find(&thread_ready_list, &cur->general_tag) );
        list_append(&thread_ready_list, &cur->general_tag);
        cur->ticks = cur->priority;

        //重新将当前线程的ticks 在重置为其 priority
        cur->status = TASK_READY;
    }else {

        //若此线程需要某事件发生后才能继续上CPU运行
        //不需要将其加入队列，因为当前线程不在就绪队列中
    }

    //如果就绪队列中没有可运行的任务，就唤醒idle 
    if(list_empty(&thread_ready_list)) {

        thread_unblock(idle_thread);
    }

    ASSERT( !list_empty(&thread_ready_list) );
    thread_tag = NULL;                          //thread_tag 清空

    //将thread_ready_list 队列中的第一个就绪线程弹出，准备将其调度上CPU
    thread_tag = list_pop(&thread_ready_list);
   // struct task_struct* next = elem2entry(struct task_struct, general_tag, thread_tag);
    struct task_struct* next = elem2entry(struct task_struct, general_tag, thread_tag);
    next->status = TASK_RUNNING;

    //激活任务页表等
    process_activate(next);

    switch_to(cur, next);
}

//当前线程将自己阻塞，标志其状态为 stat  
void thread_block(enum task_status stat) {

    //stat 取值为 TASK_BLOCK, TASK_WAITING, TASK_HANGING ,只有这三种状态不会被调度
    ASSERT(((stat == TASK_BLOCKED) || (stat == TASK_WAITING) || (stat == TASK_HANGING)));

    enum intr_status old_status = intr_disable();
    struct task_struct* cur_thread = running_thread();
    cur_thread->status = stat;                         //将其状态置为 stat     
    schedule();                                         //将当前线程换下处理器
    //待当前线程被解除阻塞后才继续运行下面的intr_set_status;
    intr_set_status(old_status);
}


//将线程pthread解除阻塞, pthread 指向已经被阻塞的线程
void thread_unblock(struct task_struct* pthread) {

    enum intr_status old_status = intr_disable();

    ASSERT(((pthread->status == TASK_BLOCKED) || (pthread->status == TASK_WAITING) || (pthread->status == TASK_HANGING)));

    if(pthread->status != TASK_READY) {

        ASSERT(!elem_find(&thread_ready_list, &pthread->general_tag));
        if(elem_find(&thread_ready_list, &pthread->general_tag)) {

            PANIC("thread_unblock: blocked thread in ready in ready_list\n");
        }

        list_push(&thread_ready_list, &pthread->general_tag);           //放到队列的最前面，使其尽快得到调度
        pthread->status = TASK_READY;
    }

    intr_set_status(old_status);
}

//主动让出cpu, 换其他线程运行
void thread_yield(void) {

    struct task_struct* cur = running_thread();
    enum intr_status old_status = intr_disable();

    ASSERT(!elem_find(&thread_ready_list, &cur->general_tag));      
    list_append(&thread_ready_list, &cur->general_tag);                 //将当前线程加入到就绪队列中，然后重新调度
    cur->status = TASK_READY;

    schedule();

    intr_set_status(old_status);
}


//初始化线程环境
void thread_init(void) {

    put_str("thread_init start\n");

    list_init(&thread_ready_list);
    list_init(&thread_all_list);
    lock_init(&pid_lock);
    /*先创建第一个用户进程: init*/
    process_execute(init, "init");
    //将当前main函数创建为线程 
    make_main_thread();
    idle_thread = thread_start("idle", 10, idle, NULL);
    put_str("thread_init done\n");
}
