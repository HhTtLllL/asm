///////////////////////////////////////////////////////////////
// File Name: syscall.c
// Author: tt
// Email: tttt@xiyoulinux.org
// Created Time : 2020-10-21 13:00:52
// Description:
///////////////////////////////////////////////////////////////
#include "syscall.h"

//无参数的系统调用
#define _syscall0(NUMBER) ({    \
    int retval;                 \
    asm volatile(               \
    "int $0x80"                 \
    : "=a" (retval)             \
    : "a"  (NUMBER)             \
    : "memory"                  \
    );                          \
    retval;                     \
})


//最多支持3个参数的系统调用 
//以下宏中 最后一个语句是整个大括号的返回值
#define _syscall1(NUMBER, ARG1) ({  \
    int retval;                     \
    asm volatile (                  \
    "int $0x80"                     \
    : "=a" (retval)                 \
    : "a" (NUMBER), "b" (ARG1)      \
    : "memory"                      \
    );                              \
    retval;                         \
})

#define _syscall2(NUMBER, ARG1, ARG2) ({    \
    int retval;                             \
    asm volatile (                          \
    "int $0x80"                             \
    : "=a" (retval)                         \
    : "a" (NUMBER), "b" (ARG1),"c" (ARG2)   \
    : "memory"                              \
    );                                      \
    retval;                                 \
})

#define _syscall3(NUMBER, ARG1, ARG2, ARG3) ({              \
    int retval;                                             \
    asm volatile (                                          \
    "int $0x80"                                             \
    : "=a" (retval)                                         \
    : "a" (NUMBER), "b" (ARG1), "c" (ARG2), "d" (ARG3)      \
    : "memory"                                              \
    );                                                      \
    retval;                                                 \
})



//返回当前任务的pid 
uint32_t getpid() {

    return _syscall0(SYS_GETPID);
}
