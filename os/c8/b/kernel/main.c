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

int main(void)
{
    put_str("I am kernel\n");
    init_all();
    //asm volatile("sti");                //为演示中断处理,在此临时开中断
   
    ASSERT(1 == 2);
    while(1);

    return 0;
}
