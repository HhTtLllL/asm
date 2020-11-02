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
#include "stdio.h"
#include "../fs/fs.h"
#include "../lib/string.h"

void k_thread_a(void*);
void k_thread_b(void*);

void u_prog_a(void);
void u_prog_b(void);

int prog_a_pid = 0, prog_b_pid = 0;
//int test_var_a = 0, test_var_b = 0;
int main(void) {
   put_str("I am kernel\n");
   init_all();
/*   process_execute(u_prog_a, "u_prog_a");
   process_execute(u_prog_b, "u_prog_b");
   thread_start("k_thread_a", 31, k_thread_a, "I am thread_a");
   thread_start("k_thread_b", 31, k_thread_b, "I am thread_b");
*/
  /* uint32_t fd = sys_open("/file2", O_RDWR);
   printf("open /file2, fd:%d\n", fd);
   char buf[64] = {0};
   int read_bytes = sys_read(fd, buf, 18);
   printf("1_ read %d bytes:\n%s\n", read_bytes, buf);

   memset(buf, 0, 64);
   read_bytes = sys_read(fd, buf, 6);
   printf("2_ read %d bytes:\n%s", read_bytes, buf);

   memset(buf, 0, 64);
   read_bytes = sys_read(fd, buf, 6);
   printf("3_ read %d bytes:\n%s", read_bytes, buf);

   printf("________  close file1 and reopen  ________\n");
   sys_lseek(fd, 0, SEEK_SET);
   memset(buf, 0, 64);
   read_bytes = sys_read(fd, buf, 24);
   printf("4_ read %d bytes:\n%s", read_bytes, buf);

   sys_close(fd);*/

   printf("/file2delete %s!\n", sys_unlink("/file2") == 0 ? "done" : "fail");
   while(1);
   return 0;
}


#if 0
int main(void)
{
    put_str("I am kernel\n");
    init_all();

//    intr_enable();

 //   process_execute(u_prog_a, "user_prog_a");
   // process_execute(u_prog_b, "user_prog_b");


/*    console_put_str(" main_pid:0x");
    console_put_int(sys_getpid());
    console_put_char('\n');
*/
    //thread_start("comsumer_a", 31, k_thread_a, "i am thread_a");
    //thread_start("comsumer_b", 31, k_thread_b, "i am thread_b");
  
    uint32_t fd = sys_open("/file2", O_RDWR);
    printf("fd:%d\n", fd);

    sys_write(fd, "hello, world\n", 12);
    sys_close(fd);

    printf("%d close now\n", fd);
    while(1);

  /*  while(1){
     
        console_put_str("Main");
};*/

    return 0;
}

#endif 
/* 在线程中运行的函数 */
void k_thread_a(void* arg) {     
   void* addr1 = sys_malloc(256);
   void* addr2 = sys_malloc(255);
   void* addr3 = sys_malloc(254);
   console_put_str(" thread_a malloc addr:0x");
   console_put_int((int)addr1);
   console_put_char(',');
   console_put_int((int)addr2);
   console_put_char(',');
   console_put_int((int)addr3);
   console_put_char('\n');

   int cpu_delay = 100000;
   while(cpu_delay-- > 0);
   sys_free(addr1);
   sys_free(addr2);
   sys_free(addr3);
   while(1);
    /*
    char* para = arg;
    void* addr1;
    void* addr2;
    void* addr3;
    void* addr4;
    void* addr5;
    void* addr6;
    void* addr7;
    console_put_str(" thread_a start\n");
    int max = 1000;
    while (max-- > 0) {
        int size = 128;
        addr1 = sys_malloc(size); 
        size *= 2; 
        addr2 = sys_malloc(size); 
        size *= 2; 
        addr3 = sys_malloc(size);
        sys_free(addr1);
        addr4 = sys_malloc(size);
        size *= 2; size *= 2; size *= 2; size *= 2; 
        size *= 2; size *= 2; size *= 2; 
        addr5 = sys_malloc(size);
        addr6 = sys_malloc(size);
        sys_free(addr5);
        size *= 2; 
        addr7 = sys_malloc(size);
        sys_free(addr6);
        sys_free(addr7);
        sys_free(addr2);
        sys_free(addr3);
        sys_free(addr4);
    }
    console_put_str(" thread_a end\n");
    while(1);
    */
    /*
       char* para = arg;
       console_put_str(" thread_a_pid:0x");
       console_put_int(sys_getpid());
       console_put_char('\n');
       console_put_str(" prog_a_pid:0x");
       console_put_int(prog_a_pid);
       console_put_char('\n');
       while(1);

       char* para = arg;
       void* addr = sys_malloc(33);
       console_put_str("i am thread_a, sys_malloc(33), addr is 0x");
       console_put_int((int)addr);
       console_put_char('\n');
       while(1);

*/
}

/* 在线程中运行的函数 */
void k_thread_b(void* arg) {     

    void* addr1 = sys_malloc(256);
   void* addr2 = sys_malloc(255);
   void* addr3 = sys_malloc(254);
   console_put_str(" thread_b malloc addr:0x");
   console_put_int((int)addr1);
   console_put_char(',');
   console_put_int((int)addr2);
   console_put_char(',');
   console_put_int((int)addr3);
   console_put_char('\n');

   int cpu_delay = 100000;
   while(cpu_delay-- > 0);
   sys_free(addr1);
   sys_free(addr2);
   sys_free(addr3);
   while(1);
    /*
    char* para = arg;
    void* addr1;
    void* addr2;
    void* addr3;
    void* addr4;
    void* addr5;
    void* addr6;
    void* addr7;
    void* addr8;
    void* addr9;
    int max = 1000;
    console_put_str(" thread_b start\n");
    while (max-- > 0) {

        int size = 9;
        addr1 = sys_malloc(size);
        size *= 2;
        addr2 = sys_malloc(size);
        size *= 2;
        sys_free(addr2);
        addr3 = sys_malloc(size);
        sys_free(addr1);
        addr4 = sys_malloc(size);
        addr5 = sys_malloc(size);
        addr6 = sys_malloc(size);
        sys_free(addr5);
        size *= 2;
        addr7 = sys_malloc(size);
        sys_free(addr6);
        sys_free(addr7);
        sys_free(addr3);
        sys_free(addr4);

        size *= 2; size *= 2; size *= 2;
        addr1 = sys_malloc(size);
        addr2 = sys_malloc(size);
        addr3 = sys_malloc(size);
        addr4 = sys_malloc(size);
        addr5 = sys_malloc(size);
        addr6 = sys_malloc(size);
        addr7 = sys_malloc(size);
        addr8 = sys_malloc(size);
        addr9 = sys_malloc(size);
        sys_free(addr1);
        sys_free(addr2);
        sys_free(addr3);
        sys_free(addr4);
        sys_free(addr5);
        sys_free(addr6);
        sys_free(addr7);
        sys_free(addr8);
        sys_free(addr9);
    }

    console_put_str(" thread_b end\n");
    while(1); */
    /*
    char* para = arg;
    void* addr = sys_malloc(63);
    console_put_str("i am thread_a, sys_malloc(63), addr is 0x");
    console_put_int((int)addr);
    console_put_char('\n');
    while(1);

    
    char* para = arg;
    console_put_str(" thread_b_pid:0x");
    console_put_int(sys_getpid());
    console_put_char('\n');
    while(1);
   */ 
}
//测试用户进程
void u_prog_a() {
    
       void* addr1 = malloc(256);
   void* addr2 = malloc(255);
   void* addr3 = malloc(254);
   printf(" prog_a malloc addr:0x%x,0x%x,0x%x\n", (int)addr1, (int)addr2, (int)addr3);

   int cpu_delay = 100000;
   while(cpu_delay-- > 0);
   free(addr1);
   free(addr2);
   free(addr3);
   while(1);
    /*char* name = "prog_a";
    //prog_a_pid = getpid();
    //console_put_str("process a getpid\n");
    printf("I am %s, my pid:%d%c", name, getpid(), '\n');
    //prog_a_pid = sys_getpid();
    while(1) {
    }*/
}

void u_prog_b(void) {
       void* addr1 = malloc(256);
   void* addr2 = malloc(255);
   void* addr3 = malloc(254);
   printf(" prog_b malloc addr:0x%x,0x%x,0x%x\n", (int)addr1, (int)addr2, (int)addr3);

   int cpu_delay = 100000;
   while(cpu_delay-- > 0);
   free(addr1);
   free(addr2);
   free(addr3);
   while(1);
    /*
    char* name = "prog_b";
    printf("I am %s, my pid:%d%c", name, getpid(), '\n');
    while(1) {
    }*/
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
