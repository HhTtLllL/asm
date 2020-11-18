///////////////////////////////////////////////////////////////
// File Name: tss.c
// Author: tt
// Email: tttt@xiyoulinux.org
// Created Time : 2020-10-18 15:02:44
// Description:
///////////////////////////////////////////////////////////////
#include "tss.h"
#include "stdint.h"
#include "../kernel/global.h"
#include "string.h"
#include "../lib/kernel/print.h"


// 任务状态段tss结构 
struct tss {
    uint32_t backlink;
    uint32_t* esp0;
    uint32_t ss0;
    uint32_t* esp1;
    uint32_t ss1;
    uint32_t* esp2;
    uint32_t ss2;
    uint32_t cr3;
    uint32_t (*eip) (void);
    uint32_t eflags;
    uint32_t eax;
    uint32_t ecx;
    uint32_t edx;
    uint32_t ebx;
    uint32_t esp;
    uint32_t ebp;
    uint32_t esi;
    uint32_t edi;
    uint32_t es;
    uint32_t cs;
    uint32_t ss;
    uint32_t ds;
    uint32_t fs;
    uint32_t gs;
    uint32_t ldt;
    uint32_t trace;
    uint32_t io_base;
}; 


 static struct tss  tss;

//更新tss中esp0字段的值为pthread的0 级栈

 void update_tss_esp(struct task_struct* pthread) {

     //也就是线程pthread 的PCB所在页的最顶端
     tss.esp0 = (uint32_t*)((uint32_t)pthread + PG_SIZE);
 }

//创建gdt 描述符
//实现: 按照段描述符的格式来拼接数据，在内部生成一个局部描述符结构体变量struct gdt_desc desc, 后面把这个结构体变量中的属性填充好后通过 
//return 返回
static struct gdt_desc make_gdt_desc(uint32_t* desc_addr, uint32_t limit, uint8_t attr_low, uint8_t attr_high) {

    uint32_t desc_base = (uint32_t)desc_addr;
    struct gdt_desc desc;
    
    desc.limit_low_word         = limit & 0x0000ffff;
    desc.base_low_word          = desc_base & 0x0000ffff;
    desc.base_mid_byte          = ((desc_base & 0x00ff0000) >> 16);
    desc.attr_low_byte          = (uint8_t)(attr_low);
    desc.limit_high_attr_high   = (((limit & 0x000f0000) >> 16) + (uint8_t)(attr_high));
    desc.base_high_byte         = desc_base >> 24;

    return desc;
}


//在gdt 中创建tss并重新加载 gdt 
//此函数用来初始化tss并将其安装到 GDT, 另外在GDT中安装两个用户进程使用的描述符， DPL为3的数据段和代码段
void tss_init() {

    put_str("tss init start\n");
    uint32_t tss_size = sizeof(tss);
    memset(&tss, 0, tss_size);

    tss.ss0 = SELECTOR_K_STACK;     //赋0级的栈段选择子
    tss.io_base = tss_size;         //当IO位图的偏移地址大于等于TSS大小减1时，就表示没有IO位图

    //gdt段基址为 0x900,把tss放到第4个位置，也就是0x900 + 0x20 的位置
    //在gdt 中添加dpl 为0 的tss描述符
    *((struct gdt_desc*)0xc0000920) = make_gdt_desc((uint32_t*)&tss, tss_size - 1, TSS_ATTR_LOW, TSS_ATTR_HIGH);

    //在gdt中添加dpl为3的数据段和代码段描述符 
    *((struct gdt_desc*)0xc0000928) = make_gdt_desc((uint32_t*)0, 0xfffff, GDT_CODE_ATTR_LOW_DPL3, GDT_ATTR_HIGH);
    *((struct gdt_desc*)0xc0000930) = make_gdt_desc((uint32_t*)0, 0xfffff, GDT_DATA_ATTR_LOW_DPL3, GDT_ATTR_HIGH);

    //gdt 16位的limit 32为的段基址
    uint64_t gdt_operand = ((8 * 7 - 1) | ((uint64_t)(uint32_t )0xc0000900 << 16));         //7个描述符大小

    asm volatile("lgdt %0" : : "m"(gdt_operand));
    asm volatile("ltr %w0" : : "r"(SELECTOR_TSS));

    put_str("tss init done\n");
}



