///////////////////////////////////////////////////////////////
// File Name: init.c
// Author: tt
// Email: tttt@xiyoulinux.org
// Created Time : 2020-09-29 18:06:26
// Description:
///////////////////////////////////////////////////////////////

#include "init.h"
#include "print.h"
#include "interrupt.h"

//负责初始化所有模块
void init_all(){

    put_str("init_all\n");
    idt_init();                     //初始化中断
}


