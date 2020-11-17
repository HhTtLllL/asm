///////////////////////////////////////////////////////////
// File Name: interrupt.c
// Author: tt
// Email: tttt@xiyoulinux.org
// Created Time : 2020-09-29 11:56:07
// Description:
///////////////////////////////////////////////////////////////
#include "interrupt.h"
#include "stdint.h"
#include "global.h"
#include "../lib/kernel/io.h"
#include "../lib/kernel/print.h"

#define PIC_M_CTRL 0x20         //这里用的是可编程中断控制器是8259a,主片的控制端口是0x20 
#define PIC_M_DATA 0x21         //主片的数据端口是0x21 
#define PIC_S_CTRL 0xa0         //从片的端口数据是0xa0 
#define PIC_S_DATA 0xa1         //从片的数据端口是0xa1 

#define IDT_DESC_CNT 0x81       //目前总共支持的中断数 
#define EFLAGS_IF 0x00000200    //eflags 寄存器中的if位为1  开中断时,eflags 寄存器中的IF的值,IF位于eflags 中的第9位,所以为0x00000200 
/* EFLAG_VAR 是c中用来存储eflags 值的变量 */
#define GET_EFLAGS(EFLAG_VAR) asm volatile("pushfl; popl %0" : "=g"(EFLAG_VAR))

extern uint32_t syscall_handler(void);          //syscall_handler 就是系统调用对应的中断入口例程

/*中断门描述符结构体*/
struct gate_desc {

    uint16_t func_offset_low_word;              //中断处理程序在目标代码段内的偏移量0 ~ 15位
    uint16_t selector;                          //中断处理程序目标代码段描述符选择子
    uint8_t dcount;                             //此项为双字计数段,是门描述符中第4字节,此项固定值 

    uint8_t attribute;          
    uint16_t func_offset_high_word;             //中断处理程序在目标段内偏移量的 16 ~ 31位
};

//静态函数声明,非必须 
//static void make_idt_desc(struct gate_desc* p_gdesc, uint8_t attr, intr_handler function);
static struct gate_desc idt[IDT_DESC_CNT];                  //idt 是中断描述符表,本质上就是个中断描述符数组

//用于保存异常的名字
char* intr_name[IDT_DESC_CNT];                              //用于保存异常的名字 

/*intr_handler 是 void* */
intr_handler idt_table[IDT_DESC_CNT];                       //定义中断处理程序数组，在kernel.asm 中定义的 intrxxentry 只是中断处理程序的入口
                                                            //,最终调用的是idt_table中的处理程序
extern intr_handler intr_entry_table[IDT_DESC_CNT];         //声明引用定义在kernel.S中的中断处理函数入口数组 


/*创建中断门描述符          中断门描述符的指针, 中断描述符内的属性    中断描述符内对应的中断处理函数*/ 
static void make_idt_desc(struct gate_desc* p_gdesc, uint8_t attr, intr_handler function) {
    
    p_gdesc->func_offset_low_word   = (uint32_t)function & 0x0000FFFF;
    p_gdesc->selector               = SELECTOR_K_CODE;                      //指向内核数据段的选择子
    p_gdesc->dcount                 = 0;
    p_gdesc->attribute              = attr;
    p_gdesc->func_offset_high_word  = ((uint32_t)function & 0xFFFF0000) >> 16;
}


//初始化中断描述符表
static void idt_desc_init(void) {

    int lastindex = IDT_DESC_CNT - 1;                       //lastindex = 0x80 ,

    for(int i = 0; i < IDT_DESC_CNT; i ++) {
                      //中断门描述符的指针，　中段描述符内的属性，　中断描述符内对应的处理函数
        //idt　中断描述符表
        make_idt_desc(&idt[i], IDT_DESC_ATTR_DPL0, intr_entry_table[i]);
    }

    //单独处理系统调用，系统调用对应的中断门 dpl为3 
    //中断处理程序为单独的syscall_handler 
    //给0x80 中断向量对应的中断描述符,在描述符中注册的中断处理例程为syscall_handler
    make_idt_desc(&idt[lastindex], IDT_DESC_ATTR_DPL3, syscall_handler);
    put_str("    idt_desc_init done\n");
}

//初始化可编程中断控制器8259A  
static void pic_init(void) {

    //初始化主片
    outb(PIC_M_CTRL, 0x11);             //ICW1: 边沿触发,级联8259, 需要ICW4
    outb(PIC_M_DATA, 0x20);             //ICW2: 起始中断向量号为0x20,也就是IR[0 - 7] 为0x20 ~ 0x27
    outb(PIC_M_DATA, 0x04);             //ICW3: IR2接从片
    outb(PIC_M_DATA, 0x01);             //ICW4: 8086模式,正常EOI 

    //初始化从片
    outb(PIC_S_CTRL, 0x11);             //ICW1: 边沿出发,级联8259,需要ICW4
    outb(PIC_S_DATA, 0x28);             //ICW2: 起始中断向量号为0x28,也就是IR[8 - 15]为0x28 ~ 0x2F 
    outb(PIC_S_DATA, 0x02);             //ICW3: 设置从片连接到主片的IR2引脚
    outb(PIC_S_DATA, 0x01);             //ICW4: 8086模式, 正常EOI 
    
    //打开主片上IR0, 也就是只介绍时钟产生的中断 
//    outb(PIC_M_DATA, 0xfe);
  //  outb(PIC_S_DATA, 0xff);
    
    //outb(PIC_M_DATA, 0xfd);
    //outb(PIC_S_DATA, 0xff);
    
    //只打开时钟和键盘中断，其他全部关闭
   // outb(PIC_M_DATA, 0xfc);
    //outb(PIC_S_DATA, 0xff);
    
    //IRQ2用于级联从片，必须打开，否则无法相应从片上的中断，
    //主片上打开的中断有IRQ0的时钟，IRQ1的键盘和级联从片的IRQ2，其他全部关闭
    outb(PIC_M_DATA, 0xf8);

    //打开从片上的IRQ14,此引脚接受硬盘控制器的中断
    outb(PIC_S_DATA, 0xbf);


    put_str("    pic_init done\n");

}

//初始化中断描述符表,向中断表中填充   中断描述符表
//通用的中断处理函数,一般用在异常出现时的处理
static void general_intr_handler(uint8_t vec_nr) {

    if (vec_nr == 0x27 || vec_nr == 0x2f) {  //0x2f是从片8259A上的最后一个irq引脚,保留IRQ7和IRQ15会产生伪中断,无需处理
       
        return ;
    }

    //将光标置为0，从屏幕左上角清出一片打印异常信息的取余，方便阅读
    set_cursor(0);                      //设置光标的值
    int cursor_pos = 0;
    while( cursor_pos < 320 ) {         //为方便阅读，在输出异常信息之前先通过while循环清空4行内容，也就是填入4行空格，一行80个
                                        //共320个空格
        put_char(' ');
        cursor_pos++;
    }

    set_cursor(0);                      //重置光标为屏幕左上角
    put_str("!!!!    excetion message begin !!!!!! \n");
    set_cursor(88);                     //从第二行第8个字符开始打印 
    put_str(intr_name[vec_nr]);

    if( vec_nr == 14 ) {            //若为Pagefault, 将缺失的地址打印出来并悬停
        
        int page_fault_vaddr = 0;
        asm ("movl %%cr2, %0" : "=r"(page_fault_vaddr));    //cr2 是存放造成page_fault的地址 
        put_str("\n page fault addr is "); put_int(page_fault_vaddr);

    }

    put_str("\n !!!!!! excetion message end  !!!!!\n");

    //能进入中断处理程序就表示已经在关中断情况下 ,不会出现调度进程的情况。故下面的死循环不会再被中断 
    
    while(1);
}


//完成一般中断处理函数注册及异常名称注册
static void exception_init(void) {

    /* idt_table 数组中的函数是在进入中断后根据中断向量号调用的,见kernel/kernel.asm 的call [idt_table + %1*4] */
    for(int i = 0; i < IDT_DESC_CNT; i ++ ){
    
        /*idt_table 中存储的是处理中断的函数 */
        //默认为general_intr_handler 
        //以后会由register_handler 来注册具体处理函数 
        idt_table[i] = general_intr_handler; 
        
        intr_name[i] = "unknown";           //先统一赋值为unknown
    }

    intr_name[0]  = "#DE Divide Error";                 //出发错误
    intr_name[1]  = "#DB Debug Exception";
    intr_name[2]  = "NMI Interrupt";                    //不可屏蔽中断
    intr_name[3]  = "#BP Breakpoint Exception";
    intr_name[4]  = "#OF Overflow Exception";
    intr_name[5]  = "#BR Bound Range Exceeded Exception";
    intr_name[6]  = "#UD Invaild Opcode Exception";
    intr_name[7]  = "#NM Device Not Available Exception";
    intr_name[8]  = "#DF Double Fault Exception";
    intr_name[9]  = "#Coprocessor Segment Overrun";
    intr_name[10] = "#TS Invalid TSS Exception";
    intr_name[11] = "#NP Segment Not Present";
    intr_name[12] = "#SS Stack Fault Exception";
    intr_name[13] = "#GP General Protection Exception";
    intr_name[14] = "#PF Page-Fault Exception";
  //  intr_name[15] 
    intr_name[16] = "#MF x87 FPU Floating-Point Error";
    intr_name[17] = "#AC Alignment Check Exception";
    intr_name[18] = "#MC Machine-Check Exception";
    intr_name[19] = "#XF SIMD Floating-Point Exception";
}


//开中断并返回开中断之前的状态
enum intr_status intr_enable(){

    enum intr_status old_status;

    if (INTR_ON == intr_get_status()) {

        old_status = INTR_ON;

        return old_status;
    }else {

        old_status = INTR_OFF;
        asm volatile("sti");            //开中断指令

        return old_status;
    }
}

//关中断,并且返回关中断前的状态 
enum intr_status intr_disable() {

    enum intr_status old_status;

    if (INTR_ON == intr_get_status()) {

        old_status = INTR_ON; 
        asm volatile("cli" : : : "memory");         //关中断,cli指令将IF位置为0

        return old_status;
    }else {

        old_status = INTR_OFF;

        return old_status;
    }
}

//将中断状态设置为 status 
enum intr_status intr_set_status(enum intr_status status) {

    return status & INTR_ON ? intr_enable() : intr_disable();
}

//获取当前中断状态 
enum intr_status intr_get_status(){

    uint32_t eflags = 0;
    GET_EFLAGS(eflags);                 //用GET_EFLAGS 宏获取eflags寄存器的状态,存入 eflags 中

    return (EFLAGS_IF & eflags) ? INTR_ON : INTR_OFF;
}

//在中断处理程序数组第 vector_no 个元素中，则侧安装中断处理程序 function 
void register_handler(uint8_t vector_no, intr_handler function) {

    //idt_table 数组中的函数实在进入中断后根据中断向量号调用的 
    //kernel/kernel.S 的call[idt_table + %1*4] 
    idt_table[vector_no] = function;
}



//完成有关中断的所有初始化工作
//中断初始化的主函数
void idt_init() {

    put_str("idt_init start\n");
    //填充中断描述符表
    idt_desc_init();                //初始化中断描述符表
    exception_init();               //异常名初始化并注册通常的中断处理函数
    pic_init();                     //初始化8259A 

    //加载idt 
    uint64_t idt_operand = ((sizeof(idt) - 1) | ((uint64_t)(uint32_t)idt << 16));
    asm volatile("lidt %0" : : "m" (idt_operand));
    put_str("idt_init done\n");
}

