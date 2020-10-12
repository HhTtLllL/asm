///////////////////////////////////////////////////////////////
// File Name: thread/sync.c
// Author: tt
// Email: tttt@xiyoulinux.org
// Created Time : 2020-10-12 15:46:46
// Description:
///////////////////////////////////////////////////////////////

#include "sync.h"
#include "list.h"
#include "global.h"
#include "debug.h"
#include "interrupt.h"
#include <stddef.h>
#include "thread.h"
//初始化信号量
void sema_init(struct semaphore* psema, uint8_t value) {

    psema->value = value;                   //为信号量赋初值
    list_init(&psema->waiters);             //初始化信号量的等待队列
}

//初始化锁 plock    
void lock_init(struct lock* plock) {

    plock->holder = NULL;
    plock->holder_repeat_nr = 0;
    sema_init(&plock->semaphore, 1);        //信号量初值为1
}


//信号量down 操作
void sema_down(struct semaphore* psema) {

    //关中断来保证原子操作
    enum intr_status old_status = intr_disable();

    while(psema->value == 0) {              //若value==0,表示锁已经被别人持有了

        ASSERT(!elem_find(&psema->waiters, &running_thread()->general_tag));
        //当前线程不应该已经在信号量的waiters 队列中 
        if(elem_find(&psema->waiters, &running_thread()->general_tag)) {

            PANIC("sema_down : thread blocked has been in waiters_list\n");
        }

        //若信号量的值等于0,则当前线程把自己加入该锁的等该队列
        list_append(&psema->waiters, &running_thread()->general_tag);
        thread_block(TASK_BLOCKED);         //阻塞线程，直到被唤醒
    }

    //若value为1或被唤醒后梦回执行下面的代码，也就是获得了锁
    psema->value--;                         //将value 减为0，防止别的线程获取资源
    ASSERT(psema->value == 0);

    //恢复之前的中断状态
    intr_set_status(old_status);
}

//信号量的UP操作,唤醒线程
void sema_up(struct semaphore* psema) {

    //关中断，保证原子操作
    enum intr_status old_status = intr_disable();

    ASSERT(psema->value == 0);
    //在信号量的等待队列中，取出队首的第一个线程，存储到thread_blocked 中
    if(!list_empty(&psema->waiters)) {

        struct task_struct* thread_blocked = elem2entry(struct task_struct, general_tag, list_pop(&psema->waiters));
        thread_unblock(thread_blocked);
    }

    psema->value++;
    ASSERT(psema->value == 1);
    //恢复之前的中断状态
    intr_set_status(old_status);
}

//获取锁 plock 
void lock_acquire(struct lock* plock){

    //排除自己已经持有的锁但还未将其释放的情况
    //判断自己是否已经成为该锁的持有者
    if(plock->holder != running_thread()) {

        sema_down(&plock->semaphore);                   //对信号量P操作，院子操作
        plock->holder = running_thread();
        ASSERT(plock->holder_repeat_nr == 0);

        plock->holder_repeat_nr = 1;
    }else {

        plock->holder_repeat_nr++;
    }
}

//释放锁plock,plock 指向待释放的锁,释放锁并不在关中断的环境下进行，所以此操作不是原子操作
void lock_release(struct lock* plock) {

    ASSERT(plock->holder == running_thread());
    //如果>1,说明自己多次申请锁，此时还不能将锁释放，只能减掉 一层锁
    if(plock->holder_repeat_nr > 1) {

        plock->holder_repeat_nr--;
        return ;
    }

    ASSERT(plock->holder_repeat_nr == 1);
    /* 置 NULL 操作要放在 sema_up 前面， 因为如果先将 将信号量+1,此时被换下处理器，新线程获取到锁之后，还未释放锁有被换下了处理器，
     * 此时执行置 NULL 操作，另一个线程操作就完全混乱了
     * */
    plock->holder = NULL;                           //把锁的持有者置空放在V操作之前
    plock->holder_repeat_nr = 0;
    sema_up(&plock->semaphore);                     //信号量的v操作，也是原子操作

}

