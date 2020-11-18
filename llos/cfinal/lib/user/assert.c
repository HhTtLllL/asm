///////////////////////////////////////////////////////////////
// File Name: assert.c
// Author: tt
// Email: tttt@xiyoulinux.org
// Created Time : 2020-11-04 15:36:29
// Description:
///////////////////////////////////////////////////////////////

#include "assert.h"
#include "../stdio.h"

void user_spin(char* filename, int line, const char* func, const char* condition) {

    printf("\n\n\nfilename %s \nline %d \nfunction %s\n condition %s\n", filename, line, func, condition);
    while(1);
}
