///////////////////////////////////////////////////////////////
// File Name: ide.c
// Author: tt
// Email: tttt@xiyoulinux.org
// Created Time : 2020-10-25 15:38:15
// Description:
///////////////////////////////////////////////////////////////
#include "ide.h"
#include "../thread/sync.h"
#include "stdio.h"
#include "../lib/kernel/stdio-kernel.h"
#include "../kernel/interrupt.h"
#include "memory.h"
#include "../kernel/debug.h"
#include "string.h"

/* 定义硬盘各寄存器的端口号 */
#define reg_data(channel)	 (channel->port_base + 0)
#define reg_error(channel)	 (channel->port_base + 1)
#define reg_sect_cnt(channel)	 (channel->port_base + 2)
#define reg_lba_l(channel)	 (channel->port_base + 3)
#define reg_lba_m(channel)	 (channel->port_base + 4)
#define reg_lba_h(channel)	 (channel->port_base + 5)
#define reg_dev(channel)	 (channel->port_base + 6)
#define reg_status(channel)	 (channel->port_base + 7)
#define reg_cmd(channel)	 (reg_status(channel))
#define reg_alt_status(channel)  (channel->port_base + 0x206)
#define reg_ctl(channel)	 reg_alt_status(channel)

/* reg_alt_status寄存器的一些关键位 */
#define BIT_STAT_BSY        0x80            // 硬盘忙
#define BIT_STAT_DRDY	    0x40            // 驱动器准备好
#define BIT_STAT_DRQ	    0x80            // 数据传输准备好了

/* device寄存器的一些关键位 */
#define BIT_DEV_MBS	        0xa0	        // 第7位和第5位固定为1
#define BIT_DEV_LBA	        0x40
#define BIT_DEV_DEV	        0x10

/* 一些硬盘操作的指令 */
#define CMD_IDENTIFY	    0xec	        // identify指令
#define CMD_READ_SECTOR	    0x20            // 读扇区指令
#define CMD_WRITE_SECTOR    0x30	        // 写扇区指令

/* 定义可读写的最大扇区数,调试用的 */
#define max_lba ((80*1024*1024/512) - 1)	// 只支持80MB硬盘

uint8_t channel_cnt;	   // 按硬盘数计算的通道数
struct ide_channel channels[2];	 // 有两个ide通道



/* 选择读写的硬盘  hd--硬盘指针,功能是选择待操作的硬盘是主盘还是从盘*/
static void select_disk(struct disk* hd) {

    uint8_t reg_device = BIT_DEV_MBS | BIT_DEV_LBA; //先拼凑出device 的值
    if(hd->dev_no == 1) {           //若是从盘就置DEV位为1

        reg_device |= BIT_DEV_DEV;
    }

    //写入 硬盘所在通道的device 寄存器
    outb(reg_dev(hd->my_channel), reg_device);
}

/* 
 * 参数: 硬盘指针hd, 扇区起始地址lba, 扇区数 sec_cnt
 * 功能: 向硬盘控制器写入起始扇区地址及要读写的扇区数
 * 
*/
static void select_sector(struct disk* hd, uint32_t lba, uint8_t sec_cnt) {

    ASSERT(lba <= max_lba);

    struct ide_channel* channel = hd->my_channel;
    
    //写入要读写的扇区数
    outb(reg_sect_cnt(channel), sec_cnt);               //如果sec_cnt 为0，则表示写入256个扇区
    //写入lba地址，即扇区号
    outb(reg_lba_l(channel), lba);                      //lba地址的低8位，不用单独取出低8位,outb函数中的汇编指令outb %b0, %w1会只用al
    outb(reg_lba_m(channel), lba >> 8);                 //lba地址的8~15位
    outb(reg_lba_h(channel), lba >> 16);                //lba地址的16~23位  


    /*因为lba地址的第24~27位要存储在Device寄存器的 0~3位
     *无法单独写入这4位，所以在此处把device寄存器再重新写入一次*/
    outb(reg_dev(channel), BIT_DEV_MBS | BIT_DEV_LBA | (hd->dev_no == 1 ? BIT_DEV_DEV : 0) | lba >> 24);
}

/*向通道 channel 发命令cmd
 *参数: 通道channel, 硬盘操作命令 cmd 
 *功能: 向通道channel 发送硬盘操作命令 cmd 
 * */ 
static void cmd_out(struct ide_channel* channel, uint8_t cmd) {

    /*只要向硬盘发出了命令便将此标记置为 true 
     *硬盘中断处理程序需要根据它来判断
    */
    channel->expecting_intr = true;                     //表明该通道发出的中断信号也许是由此次命令操作引起的，因此此通道正在期待来自硬盘的中断
    outb(reg_cmd(channel), cmd);                        //将cmd 命令cmd写入通道的cmd寄存器
}

/*硬盘读入sec_cnt 个扇区的数据到buf
 *参数:hd->代操作的硬盘,buf->缓冲器, sec_cnt->扇区数
 *功能:从硬盘hd中读入sec_cnt个扇区的数据到buf 
 * */
static void read_from_sector(struct disk* hd, void* buf, uint8_t sec_cnt)  {

    uint32_t size_in_byte;

    if(sec_cnt == 0) {
        
        /*因为sec_cnt 是8位变量，由主调函数将其赋值时，若为256 则会将最高位的1丢掉变为0*/
        size_in_byte = 256 * 512;
    }else {

        size_in_byte = sec_cnt * 512;
    }

    insw(reg_data(hd->my_channel), buf, size_in_byte / 2);      //insw 操作的是字，所以除以2
}

/*将buf 中sec_cnt扇区的数据写入硬盘
 *参数: hd->硬盘, buf->缓冲区, sec_cnt->扇区数
 *功能: 将buf中sec_cnt扇区的数据写入硬盘hd
 * */
static void write2sector(struct disk* hd, void* buf, uint8_t sec_cnt) {

    uint32_t size_in_byte;
    if (sec_cnt == 0) {

        /*因为sec_cnt是8位变量,有主调函数将其赋值时，若为256则会将最高位的1丢掉变为 0*/
        size_in_byte = 256 * 512;
    }else {

        size_in_byte = sec_cnt * 512;
    }
    outsw(reg_data(hd->my_channel), buf, size_in_byte / 2 );
}

/*等待 30 秒
 *参数: 硬盘->hd 
 *功能: 等待硬盘30秒
 * */
static bool busy_wait(struct disk* hd) {

    struct ide_channel* channel = hd->my_channel;
    uint16_t time_limit = 30 * 1000;                    //可以等待 30000 毫秒

    while(time_limit -= 10 >= 0){

        if(!(inb(reg_status(channel)) & BIT_STAT_BUSY)) {       //通过宏BIT_STAT_BSY 判断status寄存器的BSY位是否为1
                                                                //如果为1，表示硬盘繁忙(还正在读或写硬盘) 继续等待
                                                                //如果为0, 表示BSY位为0，表示硬盘不忙,再次读取status寄存器,返回其
                                                                //DRQ位的值，DRQ位为1表示硬盘已经准备好数据了
            return (inb(reg_status(channel)) & BIT_STAT_DRQ);
        }else {

            mtime_sleep(10);                            //睡眠10毫秒
        }
    }

    return false;
}



/*硬盘读取sec_cnt 个扇区到buf*/ 
void ide_read(struct disk* hd, uint32_t lba, void* buf, uint32_t sec_cnt) {

    ASSERT(lba <= max_lba);
    ASSERT(sec_cnt > 0);

    lock_acquire(&hd->my_channel->lock);

    /*1 先选择操作的硬盘*/
    select_disk(hd);

    uint32_t secs_op;                               //每次操作的扇区数
    uint32_t secs_done = 0;                         //已经完成的扇区数

}
















//硬盘数据结构初始化
void ide_init() {

    printk("ide_init start\n");

    uint8_t hd_cnt = *((uint8_t*)(0x475));  //获取硬盘的数量,在地址0x475处获取硬盘的数量，将其存入变量hd_cnt中
                                            //低端1MB以内的虚拟地址和物理地址相同,所以虚拟地址0x457可以正确访问到物理地址0x457
    ASSERT(hd_cnt > 0);

    channel_cnt = DIV_ROUND_UP(hd_cnt, 2);  //一个ide通道上有两个硬盘，根据硬盘数量反推有几个ide通道

    struct ide_channel* channel;
    uint8_t channel_no = 0; 

    //处理每个通道上的硬盘
    while(channel_no < channel_cnt) {

        channel = &channels[channel_no];
        sprintf(channel->name, "ide%d", channel_no);

        //为每个ide通道初始化端口基址及中断向量
        switch(channel_no) {

        case 0:
            channel->port_base = 0x1f0;     //ide0通道的起始端口号是0x1f0
            channel->irq_no    = 0x20 + 14; //从片8259A上倒数第二的中断引脚
                                            //硬盘，也就是ide0通道的中断向量号
            break;

        case 1:
            channel->port_base = 0x170;     //ide1通道的起始端口号是0x170   
            channel->irq_no    = 0x20 + 15; //从8259a上的最后一个中断引脚,我们用来相应ide1通道上的硬盘中断

            break;
        }

        channel->expecting_intr = false;    //未向硬盘写入指令时不期待硬盘的中断
        
        lock_init(&channel->lock);

        /*初始化为0，目的是向硬盘控制器请求数据后，硬盘驱动sema_down此信号量会阻塞线程，直到硬盘完成后通过发中断,由
         * 中断处理程序将此信号量sema_up，唤醒线程*/\
        sema_init(&channel->disk_done, 0);
        channel_no++;                       //下一个channel 
    }

    printk("ide_init done\n");
}






