///////////////////////////////////////////////////////////////
// File Name: lib/stdio.c
// Author: tt
// Email: tttt@xiyoulinux.org
// Created Time : 2020-10-21 17:36:56
// Description:
///////////////////////////////////////////////////////////////
#include "string.h"
#include "stdio.h"
#include "../kernel/interrupt.h"
#include "syscall.h"
#include "kernel/print.h"
#include "../kernel/global.h"

#define va_start(ap, v) ap = (va_list)&v            //把ap指向第一个固定参数v
#define va_arg(ap, t) *((t*)(ap += 4))              //ap 指向下一个参数并返回其值
#define va_end(ap) ap = NULL                        //清除ap 

//将整形转换成字符 (integer to ascii) 
static void itoa(uint32_t value, char** buf_ptr_addr, uint8_t base) {
    
    uint32_t m = value % base;                      //求模，从低位到高位 
    uint32_t i = value / base;                      //取整

    if(i) {                                         //如果整数不为0，递归调用
        itoa(i, buf_ptr_addr, base);
    }
    if(m < 10) {                                    //如果余数是 0 ~ 9
    
        *((*buf_ptr_addr)++) = m + '0';             //将数字0~9 转换成字符0~9
    }else {                                         //如果余数是A~F

        *((*buf_ptr_addr)++) = m - 10 + 'A';        //将数字A~F转换为字符'A' ~ 'F'
    }
}

//将参数ap按照格式format 输出到字符串 str,并返回替换后str 长度
uint32_t vsprintf(char* str, const char* format, va_list ap) {

    char* buf_ptr = str;
    const char* index_ptr = format;
    char index_char = *index_ptr;
    int32_t arg_int; 
    char* arg_str;

    while(index_char) {

        if(index_char != '%') {

            *(buf_ptr++) = index_char;
            index_char = *(++index_ptr);
            continue;
        }

        index_char = *(++index_ptr);            //得到%后面的字符
        switch(index_char) {

            case 'x':
                arg_int = va_arg(ap, int);
                itoa(arg_int, &buf_ptr, 16);
                index_char = *(++index_ptr);
                //跳过格式字符并更新index_char
                break;
            case 'd':
                arg_int = va_arg(ap, int);
                if(arg_int < 0) {

                    arg_int = 0 - arg_int;
                    *buf_ptr++ = '-';
                }

                itoa(arg_int, &buf_ptr, 10);
                index_char = *(++index_ptr);

                break;
            case 'c':
                *(buf_ptr++) = va_arg(ap, char);
                index_char = *(++index_ptr);
                break;

            case 's':
                arg_str = va_arg(ap, char*);
                strcpy(buf_ptr, arg_str);

                buf_ptr += strlen(arg_str);
                index_char = *(++index_ptr);
                break;
        }
    }

    return strlen(str);
}
//格式化输出字符串
uint32_t sprintf(char* buf, const char* format, ...) {

    va_list args;
    uint32_t retval;
    va_start(args, format);                 //使args指向format 

    retval = vsprintf(buf, format, args);        
    va_end(args);

    return retval;
}

//格式化输出字符串
uint32_t printf(const char* format, ...) {

    va_list args;
    va_start(args, format);                 //使args指向format 
    char buf[1024] = {0};                   //用于存储拼接后的字符串

    vsprintf(buf, format, args);        
    va_end(args);

    return write(buf);
}
