///////////////////////////////////////////////////////////////
// File Name: process.c
// Author: tt
// Email: tttt@xiyoulinux.org
// Created Time : 2020-10-19 11:37:37
// Description:
///////////////////////////////////////////////////////////////
#include "process.h"
#include "../kernel/global.h"
#include "../kernel/debug.h"
#include "memory.h"
#include "../thread/thread.h"    
#include "../lib/kernel/list.h"    
#include "tss.h"    
#include "../kernel/interrupt.h"
#include "string.h"
#include "../device/console.h"
#include "../kernel/memory.h"

extern void intr_exit(void);


//构建用户进程初始上下文信息
void start_process(void* filename_) {

    void* function = filename_;
    struct task_struct* cur = running_thread();
    cur->self_kstack += sizeof(struct thread_stack);

    struct intr_stack* proc_stack = (struct intr_stack*)cur->self_kstack;
    proc_stack->edi = proc_stack->esi = proc_stack->ebp = proc_stack->esp_dummy = 0;
    proc_stack->ebx = proc_stack->edx = proc_stack->ecx = proc_stack->eax = 0;

    proc_stack->gs = 0;             //用户态不需要
    proc_stack->ds = proc_stack->es = proc_stack->fs = SELECTOR_U_DATA;
    proc_stack->eip = function;     //待执行的用户程序地址
    proc_stack->cs = SELECTOR_U_CODE;
    proc_stack->eflags = (EFLAGS_IOPL_0 | EFLAGS_MBS | EFLAGS_IF_1);
    proc_stack->esp = (void*)((uint32_t)get_a_page(PF_USER, USER_STACK3_VADDR) + PG_SIZE);
    proc_stack->ss = SELECTOR_U_DATA;

    asm volatile("movl %0, %%esp; jmp intr_exit" : : "g"(proc_stack) : "memory");
}
