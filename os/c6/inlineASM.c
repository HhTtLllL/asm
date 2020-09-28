///////////////////////////////////////////////////////////////
// File Name: inlineASM.c
// Author: tt
// Email: tttt@xiyoulinux.org
// Created Time : 2020-09-28 19:05:28
// Description:
///////////////////////////////////////////////////////////////

char* str = "hello, world\n";
int count = 0;

void main(void )
{
    asm("   \
            pusha;\
            movl $4, %eax;\
            movl $1, %ebx;\
            movl str, %ecx;\
            movl $12, %edx;\
            int $0x80;\
            mov %eax, count;\
            popa\
        ");



    return ;
}


