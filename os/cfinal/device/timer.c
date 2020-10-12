///////////////////////////////////////////////////////////////
// File Name: timer.c
// Author: tt
// Email: tttt@xiyoulinux.org
// Created Time : 2020-10-06 01:03:59
// Description:
///////////////////////////////////////////////////////////////

#include "timer.h"
#include "io.h"
#include "print.h"
#include "../thread/thread.h"
#include "interrupt.h"
#include "debug.h"


#define IRQ0_FREQUENCY      100
#define INPUT_FREQUENCY     1193180 
#define COUNTER0_VALUE      INPUT_FREQUENCY / IRQ0_FREQUENCY 
#define COUNTER0_PORT       0x40 
#define COUNTER0_NO         0
#define COUNTER_MODE        2
#define READ_WRITE_LATCH    3
#define PIT_CONTROL_PORT    0x43

uint32_t ticks;                     //ticks 是内核自中断开启以来总共的滴答数

/*把操作的计数器 counter_no, 读写锁属性rwl, 计数器模式,counter_mode 写入模式控制寄存器并赋予初始值count_value*/
static void frequency_set(uint8_t counter_port, 
                          uint8_t counter_no,
                          uint8_t rwl,
                          uint8_t counter_mode,
                          uint16_t counter_value){

    //往控制字寄存器端口0x43中写入控制字
    outb(PIT_CONTROL_PORT, (uint8_t)(counter_no << 6 | rwl << 4 | counter_mode << 1));

    //先写入counter_value 的低8位 
    outb(counter_port, (uint8_t)counter_value);
    //在写入counter_value 的高8位 
    outb(counter_port, (uint8_t)counter_value >> 8);
}

//时钟的中断处理函数
static void intr_timer_handler(void) {
    
    //获取当前正在运行的线程，将其赋值给 PCB指针,cur_thread 
    struct task_struct* cur_thread = running_thread();

    ASSERT(cur_thread->stack_magic == 0x19870916);          //检查是否溢出

    cur_thread->elapsed_ticks++;                            //记录线程占用的CPU的时间，该线程总执行时间 + 1
    ticks++;                                                //将系统运行时间+1, 实际就是中断发生的次数

    if( cur_thread->ticks == 0 ) {                          //若进程时间片用完，就开始调度新的进程上CPU

        schedule();
    }else {                                                 //将当前时间片-1

        cur_thread->ticks--;
    }
}



//初始化PIT8253 
void timer_init() {     //timer_inti 在init_all 调用的，它在内核运行开始处执行的，所以时钟中断处理函数会被提前注册好的

    put_str("timer_init start\n");
    //设置8253的定时周期,也就是发中断的周期
    frequency_set(COUNTER0_PORT, COUNTER0_NO, READ_WRITE_LATCH, COUNTER_MODE, COUNTER0_VALUE);
        
    //注册时钟中断处理程序
    register_handler(0x20, intr_timer_handler);
    put_str("timer_init done\n");
}



























