///////////////////////////////////////////////////////////////
// File Name: string.c
// Author: tt
// Email: tttt@xiyoulinux.org
// Created Time : 2020-10-09 11:20:57
// Description:
///////////////////////////////////////////////////////////////

#include "string.h"
#include "global.h"
#include "debug.h"
#include <stddef.h>

//将dst 起始的size个字节置为 value 
void memset(void* dst_, uint8_t value, uint32_t size) {

//    ASSERT(dst_ != NULL);
    uint8_t* dst = (uint8_t*)dst_;

    while(size-- > 0) *dst++ = value;
}

//将 src_起始的size 个字节复制到dst_  

void memcpy(void* dst_, const void* src_, uint32_t size){

    ASSERT(dst_ != NULL && src_ != NULL);
    uint8_t* dst = dst_;
    const uint8_t* src = src_; 

    while(size-- > 0) *dst++ = *src++;
}

//连续比较以地址a_和地址b_开头的size 个字节,若相等则返回0, a_大于b_,返回+1, 否则返回-1 
int memcmp(const void* a_, const void* b_, uint32_t size){

    const char* a = a_;
    const char* b = b_;

    ASSERT(a != NULL || b != NULL);
    while(size-- > 0) {

        if(*a != *b) {

            return *a > *b ? 1 : -1;
        }
        a++;
        b++;
    }

    return 0;
}

//将字符串从src_ 复制到 dst 
char* strcpy(char* dst_, const char* src_){

    ASSERT(dst_ != NULL && src_ != NULL);
    char* r = dst_;                         //用来返回目的字符串起始地址
    while((*dst_++ = *src_++));

    return r;
}


//返回字符串长度
uint32_t strlen(const char* str) {

    ASSERT(str != NULL);
    const char* p = str;
    while(*p++);

    return (p - str - 1);
}

//比较两个字符串,若a_中的字符大于b_中的字符返回1,相等时返回0, 不相等时返回 -1 
int8_t strcmp(const char* a, const char* b) {

    ASSERT(a != NULL && b != NULL);
    while( *a != 0 && *a == *b ){
        a++;
        b++;
    }
    //如果*a 小于*b 就返回-1, 否则就属于*a大于等于*b的情况.

    return *a < *b ? -1 : *a > *b;
}

//从左到右查找str中首次出现字符ch的地址 
char* strchr(const char* str, const uint8_t ch){

    ASSERT(str != NULL);
    while(*str != '\0') {

        if(*str == ch) {

            return (char*)str;          //需要强制转化成和返回值类型一样,否则编译器会报const属性丢失
        }
        str++;
    }

    return NULL;
}

//从后往前查找字符串str中首次出现ch的地址 
char* strrchr(const char* str, const uint8_t ch) {

    ASSERT(str != NULL);
    const char* last_char = NULL;

    //从头遍历一次,若存在ch字符,last_char总是最后一次出现在串中的地址 
    while(*str != 0) {

        if(*str == ch) {

            last_char = str;
        }
        str++;
    }

    return (char*)last_char;
}

//将字符串src_ 拼接到dst_ 后,返回拼接的串地址
char* strcat(char* dst_, const char* src_) {

    ASSERT(dst_ != NULL && src_ != NULL);
    char* str= dst_;

    while(*str++);
    --str;

    while((*str++ = *src_++));          //当*str被赋值为0 
    //也就是表达式不成立,正好添加了字符串结尾的0
    return dst_;
}

//在字符串str中查找字符ch出现的次数
uint32_t strchrs(const char* str, uint8_t ch) {

    ASSERT(str != NULL);
    uint32_t ch_cnt = 0;
    const char* p = str;
    while(*p != '\0') {

        if(*p == ch) 
            ch_cnt++;
        p++;
    }

    return ch_cnt;
}


