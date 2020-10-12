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
#include "global.h"
#include "memory.h"
#include "debug.h"
#include "interrupt.h"
#include "print.h"
#include "list.h"

#define PG_SIZE 4096 

struct task_struct* main_thread;                    //主线程PCB 
struct list thread_ready_list;                      //就绪队列,就绪队列中的线程都是可用于直接上处理器运行的，阻塞的线程是不放在就绪队列中的
struct list thread_all_list;                        //所有任务队列,全部线程队列 
static struct list_elem* thread_tag;                //用于保存队列中的线程结点　

extern void switch_to(struct task_struct* cur, struct task_struct* next);

//获取当前线程PCB指针
struct task_struct* running_thread() {

    uint32_t esp;
    asm("mov %%esp, %0" : "=g"(esp));
    //取esp整数部分，即PCB起始地址 
    //各个线程所用的0级栈都是在自己的PCB当中，因此取当前栈指针的高20位作为当前运行线程PCB。
    return (struct task_struct*)(esp & 0xfffff000);
}

//由kernel_thread 去执行 function(func_arg) 
static void kernel_thread(thread_func* function, void* func_arg) {

    //执行function 前要开中断，避免后面的时钟中断被屏蔽，而无法调度其他线程 
    intr_enable();                                  //开中断
    function(func_arg);

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
    strcpy(pthread->name, name); 

    if( pthread == main_thread ) {
        //由于把main函数也封装成一个线程，并且它一直是运行的，故将其直接设为 YASK_RUNNING  
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
    pthread->stack_magic = 0x19870916;                              //自定义的魔数

}

//创建一优先级为prio的线程,线程名为 name ,线程所执行的函数是(function(func_arg))
struct task_struct* thread_start(char* name, int prio, thread_func function, void* func_arg) {

    //pcb 都位于内核空间，包括用户进程的pcb也是在内核空间
    //在内核申请一页内存,返回的是页的起始地址,所以thread　指向的是PCB的最低地址
    struct task_struct* thread = get_kernel_pages(1);
    //　将３个参数写入线程的PCB，并且完成PCB一级的其他初始化;
    init_thread(thread, name, prio);
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

    ASSERT( !list_empty(&thread_ready_list) );
    thread_tag = NULL;                          //thread_tag 清空

    //将thread_ready_list 队列中的第一个就绪线程弹出，准备将其调度上CPU
    thread_tag = list_pop(&thread_ready_list);
   // struct task_struct* next = elem2entry(struct task_struct, general_tag, thread_tag);
    struct task_struct* next = elem2entry(struct task_struct, general_tag, thread_tag);
    next->status = TASK_RUNNING;
    switch_to(cur, next);
}

//初始化线程环境

void thread_init(void) {

    put_str("thread_init start\n");

    list_init(&thread_ready_list);
    list_init(&thread_all_list);

    //将当前main函数创建为线程 
    make_main_thread();
    put_str("thread_init done\n");
}























