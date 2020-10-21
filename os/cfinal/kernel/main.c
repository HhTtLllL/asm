///////////////////////////////////////////////////////////////
// File Name: main.c
// Author: tt
// Email: tttt@xiyoulinux.org
// Created Time : 2020-09-29 19:09:54
// Description:
///////////////////////////////////////////////////////////////
#include "print.h"
#include "init.h"
#include "debug.h"
#include "../thread/thread.h"
#include "interrupt.h"
#include "console.h"
#include "../device/ioqueue.h"
#include "../device/keyboard.h"
#include "../userprog/process.h"
#include "syscall.h"
#include "syscall-init.h"

void k_thread_a(void*);
void k_thread_b(void*);

void u_prog_a(void);
void u_prog_b(void);

int prog_a_pid = 0, prog_b_pid = 0;
int test_var_a = 0, test_var_b = 0;

int main(void)
{
    put_str("I am kernel\n");
    init_all();
    
    process_execute(u_prog_a, "user_prog_a");
    process_execute(u_prog_b, "user_prog_b");

    console_put_str(" main_pid:0x");
    console_put_int(sys_getpid());
    console_put_char('\n');

    thread_start("comsumer_a", 31, k_thread_a, " A_");
    thread_start("comsumer_b", 31, k_thread_b, " B_");
    
    
    
    intr_enable();

    while(1);

  /*  while(1){
     
        console_put_str("Main");
};*/

    return 0;
}

/* 在线程中运行的函数 */
void k_thread_a(void* arg) {     

    char* para = arg;
    console_put_str(" thread_a_pid:0x");
    console_put_int(sys_getpid());
    console_put_char('\n');
    console_put_str(" prog_a_pid:0x");
    console_put_int(prog_a_pid);
    console_put_char('\n');
    while(1);

}

/* 在线程中运行的函数 */
void k_thread_b(void* arg) {     

    char* para = arg;
    console_put_str(" thread_b_pid:0x");
    console_put_int(sys_getpid());
    console_put_char('\n');
    console_put_str(" prog_b_pid:0x");
    console_put_int(prog_b_pid);
    console_put_char('\n');
    while(1);
   
}
//测试用户进程
void u_prog_a() {
    
    //prog_a_pid = getpid();
    //console_put_str("process a getpid\n");
    
    prog_a_pid = sys_getpid();
    while(1) {
    }
}

void u_prog_b(void) {

    prog_b_pid = getpid();
    while(1) {
    }
}

//在线程中运行的函数
/*
void k_thread_a (void* arg) {

    //用void* 来通用表示参数，被调用的函数知道自己需要什么类型的参数，自己转换在用
    char* para =(char*)arg;
    while(1){

       console_put_str(para);
    }
}



void k_thread_b(void* arg) {

    //用boid* 来通用表示参数，被调用的函数知道自己需要什么类型的参数，自己在转换再用
    char* para = arg;

    while(1) {

        console_put_str(para);
    }
}
*/
