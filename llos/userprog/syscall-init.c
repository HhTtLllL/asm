///////////////////////////////////////////////////////////////
// File Name: syscall-init.c
// Author: tt
// Email: tttt@xiyoulinux.org
// Created Time : 2020-10-21 13:56:18
// Description:
///////////////////////////////////////////////////////////////

#include "syscall-init.h"
#include "stdint.h"
#include "../lib/kernel/print.h"
#include "../thread/thread.h"
#include "../device/console.h"
#include "../lib/string.h"
#include "fork.h"
#include "../lib/user/syscall.h"
#include "../fs/fs.h"

#define syscall_nr 32
typedef void* syscall;

syscall syscall_table[syscall_nr];

//返回当前任务的pid 
uint32_t sys_getpid(void) {

    return running_thread()->pid;
}

//初始化系统调用
void syscall_init(void) {

    put_str("syscall_init start\n");

    syscall_table[SYS_GETPID]   = (void *)sys_getpid;
    syscall_table[SYS_WRITE]    = (void *)sys_write;
    syscall_table[SYS_MALLOC]   = (void *)sys_malloc;
    syscall_table[SYS_FREE]     = (void *)sys_free;
    syscall_table[SYS_FORK]     = (void *)sys_fork;
    syscall_table[SYS_READ]     = (void *)sys_read;
    syscall_table[SYS_PUTCHAR]  = (void *)sys_putchar;
    syscall_table[SYS_CLEAR]    = (void *)cls_screen;
    syscall_table[SYS_GETCWD]   = (void *)sys_getcwd;
    syscall_table[SYS_OPEN]     = (void *)sys_open;
    syscall_table[SYS_CLOSE]    = (void *)sys_close;
    syscall_table[SYS_LSEEK]	= (void *)sys_lseek;
    syscall_table[SYS_UNLINK]	= (void *)sys_unlink;
    syscall_table[SYS_MKDIR]	= (void *)sys_mkdir;
    syscall_table[SYS_OPENDIR]	= (void *)sys_opendir;
    syscall_table[SYS_CLOSEDIR] = (void *)sys_closedir;
    syscall_table[SYS_CHDIR]	= (void *)sys_chdir;
    syscall_table[SYS_RMDIR]	= (void *)sys_rmdir;
    syscall_table[SYS_READDIR]	= (void *)sys_readdir;
    syscall_table[SYS_REWINDDIR]= (void *)sys_rewinddir;
    syscall_table[SYS_STAT]	    = (void *)sys_stat;
    syscall_table[SYS_PS]	    = (void *)sys_ps;

    put_str("syscall_init done\n");
}
