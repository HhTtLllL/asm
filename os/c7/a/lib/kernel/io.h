#ifndef __LIB_IO_H
#define __LIB_IO_H
#include "stdint.h"

//向端口port 写入一个字节 
static inline void outb(uint16_t port, uint8_t data) {

    /*对端口指定N表示0~255,d表示yong dx存储端口号,%b0 表示对应al,%w1 表示对应dx */
    asm volatile ("outb %b0, %w1" : : "a" (data), "Nd"(port));
}

static inline void outsw(uint16_t port, const void* addr, uint32_t word_cnt) {

    /* + 表示此限制既做输入也做输出, outsw 是把ds:esi 处的16位的内容写入port端口,我们在设置
     * 段描述符时,已经将ds,es,ss段的选择子都设置为相同的值,此时不需要担心数据错乱 */

    asm volatile("cld; rep outsw" : "+S"(addr), "+c" (word_cnt) : "d"(port));
}

//将从端口port读入的一个字节返回
static inline uint8_t inb(uint16_t port) {

    uint8_t data;
    asm volatile("inb %w1, %b0" : "=a"(data) : "Nd" (port));
    return data;
}

//将从端口port读入的word_cnt个字写入addr  
static inline void insw(uint16_t port, void* addr, uint32_t word_cnt) {
    /* insw 是将从端口port处读入的16位内容写入es:edi 指向的内存, 我们在设置段描述符时,已经将ds,es,ss段的选择子都设置为相同的值,
     * 不用担心数据错乱*/

    asm volatile("cld; rep insw" : "+D"(addr), "+c"(word_cnt) : "d"(port) : "memory");
}

#endif
