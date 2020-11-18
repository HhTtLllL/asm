///////////////////////////////////////////////////////////////
// File Name: debug.c
// Author: tt
// Email: tttt@xiyoulinux.org
// Created Time : 2020-10-07 15:51:15
// Description:
///////////////////////////////////////////////////////////////

#include "debug.h"
#include "print.h"
#include "interrupt.h"

//打印文件名,行号,函数名,条件并使程序悬停 
void panic_spin(char* filename, int line, const char* func, const char* condition) {

    intr_disable();                             //因为有使用会单独调用panic_spn,所以在此处关中断
    put_str("\n\nerror !!!\n\n");
    put_str("filename: ");  put_str(filename);          put_str("\n");
    put_str("line:0x");     put_int(line);              put_str("\n");
    put_str("function:");   put_str((char*)func);       put_str("\n");
    put_str("condition:");  put_str((char*)condition);  put_str("\n");
    while(1);
}
