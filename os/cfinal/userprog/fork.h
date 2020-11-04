#ifndef __USERPROG_FORK_H
#define __USERPROG_FORK_H 

#include "../thread/thread.h"

/*fork子进程, 只能由用户进程通过系统调用 fork调用, 
 *
 *内核线程不可以直接调用,原因是要从 0 级栈中获得
 * */

pid_t sys_fork(void);


#endif
