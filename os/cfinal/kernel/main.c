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

void k_thread_a(void*);
void k_thread_b(void*);

void u_prog_a(void);
void u_prog_b(void);

int test_var_a = 0, test_var_b = 0;

int main(void)
{
    put_str("I am kernel\n");
    init_all();

    thread_start("comsumer_a", 31, k_thread_a, " A_");
    thread_start("comsumer_b", 31, k_thread_b, " B_");
    
    process_execute(u_prog_a, "user_prog_a");
    process_execute(u_prog_b, "user_prog_b");
    
    
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
    while(1) {

        console_put_str("v_a:0x");
        console_put_int(test_var_a);
    }
}

/* 在线程中运行的函数 */
void k_thread_b(void* arg) {     
   
    char* para = arg;
    while(1) {

        console_put_str(" v_b:0x");
        console_put_int(test_var_b);
    }
}
//测试用户进程
void u_prog_a() {

    while(1) {
        test_var_a++;
    }
}

void u_prog_b(void) {

    while(1) {
        test_var_b++;
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
