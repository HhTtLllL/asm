#ifndef __THREAD_THREAD_H
#define __THREAD_THREAD_H 

#include "stdint.h"
#include "../lib/kernel/list.h"
#include "../lib/kernel/bitmap.h"
#include "../kernel/memory.h"


#define MAX_FILES_OPEN_PER_PROC 8

//自定义通用函数类型，它将在很多线程函数中作为形参类型
typedef void thread_func(void*);
typedef int16_t pid_t;
//进程或线程的状态  程序的状态是通用的，所以这个结构同样也是进程的状态
enum task_status {
    
    TASK_RUNNING,
    TASK_READY,
    TASK_BLOCKED,
    TASK_WAITING,
    TASK_HANGING,
    TASK_DIED
};

/*中断栈  intr_stack
 * 此结构用于中断发生时保护程序（线程或进程）的上下文环境
 * 进程或线程被外部中断或软中断打断时，会按照此结构压入上下文
 * 寄存器, intr_exit 中的出栈操作是此结构的逆操作 
 * 初始时，此栈在线程自己的内核栈中位置固定，所在页的最顶端
 *
 * 中断入口程序intr%1entry 所执行的上下文保护的一系列压栈操作都是压入了此结构中
 * */
struct intr_stack {
    
    uint32_t vec_no;                        //kernel.S宏Vector中push %1压入的中断号 
    uint32_t edi;
    uint32_t esi;
    uint32_t ebp;
    uint32_t esp_dummy;
    //虽然pushad把esp也压入，但esp 是不断变化的，所以会被popad忽略
    uint32_t ebx;
    uint32_t edx;
    uint32_t ecx;
    uint32_t eax;
    uint32_t gs;
    uint32_t fs;
    uint32_t es;
    uint32_t ds; 

    //以下由CPU从低特权级 进入高特权级时 压入
    uint32_t err_code;                  //err_code 会被压入eip之后
    void (*eip) (void);
    uint32_t cs;
    uint32_t eflags;
    void* esp;
    uint32_t ss; 
};

/* 线程栈  thread_stack   
 * 线程自己的栈，用于存储线程中等待执行的函数
 * 此结构在线程自己的内核栈中位置不固定，仅用在 switch_to时保存线程环境
 * 实际位置取决于实际运行情况
 * */
struct thread_stack {
    /*硬件体系上的所有寄存器都具有全局性，因此在函数调用时，这些寄存器对主调函数和被调函数都可见
     *ebp, ebx, edi, esi, esp 归主调函数使用，其余的寄存器归被调函数使用。

     也就是说，不管被调函数中是否使用了这５个寄存器，在被调函数执行完后，这５个寄存器的值不能
     被改变。　因此被调函数必须为主调函数保护好这５个寄存器的值，在被调函数运行完之后，这５个寄存器的值
     必须和之前一样，所以先备份起来。
     
     至于这里为什么没有保证 esp的原因是调用约定会保证esp的值，所以此处进行备份保证
     */
    //这四个寄存器 用来保存主调函数的时 中的值
    uint32_t ebp;
    uint32_t ebx;
    uint32_t edi;
    uint32_t esi;

    //线程第一次执行时，eip指向待调用的函数kernel_thread 
    //其他时候，eip是指向switch_to 的返回地址 
    //当任务切换时此eip用于保存任务切换后的新任务的返回地址
    void (*eip) (thread_func* func, void* func_arg);
    //以下仅供第一次被调度上CPU时使用
    //参数 unused_ret 只为占位置充数为返回地址 
    void (*unused_retaddr);
    thread_func* function;                           //由kernel_thread所调用的函数名
    void *func_arg;                                  //kernel_thread 所调用的函数所需的参数
};


//进程或线程的 PCB 程序控制块 
struct task_struct {
    
    //当线程被创建时，self_kstack被初始化为自己PCB所在页的顶端。
    uint32_t* self_kstack;                  //各线程都用自己的内核栈
    pid_t pid;
    enum task_status status;                //线程状态
    uint8_t priority;                       //线程优先级
    char name[16];                          //线程或进程的名字
    uint8_t ticks;                          //每次在处理器上执行的时间滴答数
    
    //此任务自从上CPU运行后至今占用了多少CPU滴答数，也就是此任务执行了多久
    //用来记录任务在处理器上运行的时钟滴答数,从开始执行到结束执行　
    uint32_t elapsed_ticks;
    
    int32_t fd_table[MAX_FILES_OPEN_PER_PROC];                      //文件描述符数组
    //general_tag  的作用是用于线程在一般的队列中的节点
    //线程的标签，当线程被加入到就绪队列thread_ready_list 或其他等待队列中，就把该线程PCB中的general_tag的地址加入队列
    struct list_elem general_tag; 

    //all_list_tag 的作用是用于线程队列 thread_all_list 中的节点 
    //全部线程队列 thread_all_list 
    struct list_elem all_list_tag;
    

    //线程与进程的最大区别就是进程独享自己的地址空间，即进程有自己的页表，而线程共享所在进程的地址空间，即线程无页表
    //如果该任务为线程,pgdir 则为NULL,否则pgdir　会被赋予页表的虚拟地址，此处是虚拟地址，页表加载时还是要被转换成物理地址的 
    uint32_t* pgdir;                                    //进程自己页表的虚拟地址,任务自己的页表,用来存放进程页目录表的虚拟地址,
    uint32_t cwd_inode_nr;                              //进程所在的工作目录的inode编号
    int16_t parent_pid;
    uint32_t stack_magic;                               //栈的边界标记，用于检测栈的溢出
    struct virtual_addr userprog_vaddr;                 //用户空间的虚拟地址
    struct mem_block_desc u_block_desc[DESC_CNT];       //用户进程内存块描述符
};


extern struct list thread_ready_list;
extern struct list thread_all_list;

void thread_create(struct task_struct* pthread, thread_func function, void* func_arg);
void init_thread(struct task_struct* pthread, char* name, int prio);
struct task_struct* thread_start(char* name, int prio, thread_func function, void* func_arg);
struct task_struct* running_thread(void);
void schedule(void);
void thread_init(void);
void thread_block(enum task_status stat);
void thread_unblock(struct task_struct* pthread);
void thread_yield(void);
pid_t fork_pid(void);
void sys_ps(void);



#endif          //__THREAD_THREAD_H 
