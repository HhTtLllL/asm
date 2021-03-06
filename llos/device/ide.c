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
#include "../kernel/memory.h"
#include "../kernel/debug.h"
#include "string.h"
#include "../lib/kernel/io.h"
#include "console.h"
#include "timer.h"
#include "../lib/kernel/list.h"


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
#define BIT_STAT_DRQ	    0x8            // 数据传输准备好了

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


/*用于记录总扩展分区的起始lba, 初始为0,partition_scan 时以此为标记*/
int32_t ext_lba_base = 0;

uint8_t p_no = 0, l_no = 0;                 //用来记录硬盘主分区和逻辑分区的下标
struct list partition_list;                 //分区队列

/*构建一个16字节大小的结构体,用来存分区表项*/
struct partition_table_entry {

    uint8_t bootable;                       //是否可被引导
    uint8_t start_head;                     //起始磁头号
    uint8_t start_sec;                      //起始扇区号
    uint8_t start_chs;                      //起始柱面号
    uint8_t fs_type;                        //分区类型
    uint8_t end_head;                       //结束磁头号
    uint8_t end_sec;                        //结束扇区号
    uint8_t end_chs;                        //结束柱面号

    /*注意下面这两项*/

    uint32_t start_lba;                     //本分区起始扇区的lba地址
    uint32_t sec_cnt;                       //本分区的扇区数目
}__attribute__ ((packed));                  //保证次结构是16字节大小




/*引导扇区,mbr 或 ebr所在的扇区*/
struct boot_sector {

    uint8_t other[446];                     //引导代码
    struct partition_table_entry partition_table[4];      //分区表中有4项,共64字节
    uint16_t signature;                     //启动扇区的结束标志是 0x55,0xaa 
}__attribute__ ((packed));

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

        if(!(inb(reg_status(channel)) & BIT_STAT_BSY)) {       //通过宏BIT_STAT_BSY 判断status寄存器的BSY位是否为1
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

    while(secs_done < sec_cnt) {

        if((secs_done + 256) <= sec_cnt) {

            secs_op = 256;
        }else {

            secs_op = sec_cnt - secs_done;
        }

        /*2 写入待读入的扇区数 和起始扇区号*/
        select_sector(hd, lba + secs_done, secs_op);

        /*3 执行的命令写入 reg_cmd 寄存器*/
        cmd_out(hd->my_channel, CMD_READ_SECTOR);       //准备开始读数据

        /*                          阻塞自己的时机 
         *在硬盘已经开始工作(开始在内部读数据或写数据)后才能阻塞自己，现在硬盘已经开始忙了
         *将自己阻塞，等待硬盘完成读操作后通过中断处理程序唤醒自己
         */
        sema_down(&hd->my_channel->disk_done);

        /*4 检测硬盘状态是否可读*/
        //醒来后开始执行下面代码
        if(!busy_wait(hd)) {

            char error[64];
            sprintf(error, "%s read sector %d failed!!!!\n", hd->name, lba);
            PANIC(error);
        }

        /*5 把数据从硬盘的缓冲区读出*/
        read_from_sector(hd, (void*)((uint32_t)buf + secs_done * 512), secs_op);

        secs_done += secs_op;
    }

    lock_release(&hd->my_channel->lock);
}

/*buf 中sec_cnt 扇区数据写入硬盘*/ 
void ide_write(struct disk* hd, uint32_t lba, void* buf, uint32_t sec_cnt) {

    ASSERT(lba <= max_lba);
    ASSERT(sec_cnt > 0);

    lock_acquire(&hd->my_channel->lock);

    /*1 先选择操作的硬盘 */
    select_disk(hd);

    uint32_t secs_op;                   //每次操作的扇区数
    uint32_t secs_done = 0;             //已经完成的扇区数

    while(secs_done < sec_cnt) {

        if((secs_done + 256) <= sec_cnt) {

            secs_op = 256;
        }else {

            secs_op = sec_cnt - secs_done;
        }

        /*2 写入待写入的扇区数和起始扇区号*/
        select_sector(hd, lba + secs_done, secs_op);

        /*3 执行的命令写入reg_cmd 寄存器*/
        cmd_out(hd->my_channel, CMD_WRITE_SECTOR);          //准备开始写数据

        /*4 检测硬盘状态是否可读*/ 
        if(!busy_wait(hd)) {

            char error[64];
            sprintf(error, "%s write sector %d failed!!!!\n", hd->name, lba);
            PANIC(error);
        }

        /*5 将数据写入硬盘*/
        write2sector(hd, (void*)((uint32_t)buf + secs_done * 512), secs_op);

        /*在硬盘响应期间阻塞自己*/
        sema_down(&hd->my_channel->disk_done);
        secs_done += secs_op;
    }
    
    //醒来后开始释放锁
    lock_release(&hd->my_channel->lock);
}

/*将dst 中 len个相邻字节交换位置后存入 buf
 *参数: 目标数据地址dst, 缓冲区buf, 数据长度len
 *功能: 将dst中len个相邻字节交换位置后存入buf, buf是dst最终转换的结果,此函数用来处理identify命令的返回信息
        在这16位的字中,相邻字符的位置是互换的,所以通过此函数做转换.
 * */
static void swap_pairs_bytes(const char* dst, char* buf, uint32_t len) {

    uint8_t idx;
    for(idx = 0; idx < len; idx += 2) {

        /*buf 中存储dst中两相邻元素交换后的字符串*/
        buf[idx + 1] = *dst++;
        buf[idx] = *dst++;
    }

    buf[idx] = '\0';
}

/*获得硬盘参数信息
 *参数: hd->硬盘
 *功能: 向硬盘发送identify 命令以获得硬盘参数信息
 *      id_info: 用来存储向硬盘发送identify 命令后返回的硬盘参数
 * */
static void identify_disk(struct disk* hd) {

    char id_info[512];
    select_disk(hd);                        //选择要操作的硬盘
    cmd_out(hd->my_channel, CMD_IDENTIFY);

    /*向硬盘发送指令后便通过信号量阻塞自己,待硬盘处理完成后,通过中断处理程序将自己唤醒*/
    sema_down(&hd->my_channel->disk_done);

    /*醒来后开始执行下面代码*/
    if(!busy_wait(hd)) {                    //若失败
        
        char error[64];
        sprintf(error, "%s identify failed!!!\n", hd->name);
        PANIC(error);
    }

    read_from_sector(hd, id_info, 1);       //从硬盘获取信息到id_info 

    char buf[64];                           //是给swap_pairs_bytes使用的,用于存储转换结果

    //sn_start:序列号起始地址,md_start表示型号起始字节地址
    uint8_t sn_start = 10 * 2, sn_len = 20, md_start = 27 * 2, md_len = 40;
    swap_pairs_bytes(&id_info[sn_start], buf, sn_len);
    printk("disk %s info:\n  SN: %s\n", hd->name, buf);
    memset(buf, 0, sizeof(buf));

    swap_pairs_bytes(&id_info[md_start], buf, md_len);
    printk("MODULE: %s\n", buf);
    uint32_t sectors = *(uint32_t*)&id_info[60 * 2];
    printk("Sectors: %d\n", sectors);
    printk("CAPACITY: %dMB\n", sectors * 512 / 1024 / 1024);
}

/*扫描硬盘 hd 中地址为ext_lba的扇区中的所有分区*/
static void partition_scan(struct disk* hd, uint32_t ext_lba) {

    struct boot_sector* bs = sys_malloc(sizeof(struct boot_sector));
    ide_read(hd, ext_lba, bs, 1);
    uint8_t part_idx = 0;
    struct partition_table_entry* p = bs->partition_table;

    /*遍历分区表 4 个分区表项*/
    while(part_idx++ < 4) {

        if(p->fs_type == 0x5) {     //若为扩展分区

            if(ext_lba_base != 0) {

                /*自扩展分区的start_lba是相对于主引导扇区中的总扩展分区地址*/
                partition_scan(hd, p->start_lba + ext_lba_base);
            }else {

                /*ext_lba_base 为0表示是第一次读取引导块,也就是主引导记录所在的扇区
                 *记录下扩展分区的起始lba地址,后面所有的扩展分区地址都相对与此
                 * */

                ext_lba_base = p->start_lba;
                partition_scan(hd, p->start_lba);
            }
        }else if (p->fs_type != 0){                 //若是有效的分区类型

            if(ext_lba == 0) {                      //此时全是主分区

                hd->prim_parts[p_no].start_lba = ext_lba + p->start_lba;
                hd->prim_parts[p_no].sec_cnt = p->sec_cnt;
                hd->prim_parts[p_no].my_disk = hd;

                list_append(&partition_list, &hd->prim_parts[p_no].part_tag);
                sprintf(hd->prim_parts[p_no].name, \
                        "%s%d", hd->name, p_no + 1);
                p_no++;
                ASSERT(p_no < 4);                   //0,1,2,3
            }else {

                hd->logic_patrs[l_no].start_lba = ext_lba + p->start_lba;
                hd->logic_patrs[l_no].sec_cnt = p->sec_cnt;
                hd->logic_patrs[l_no].my_disk = hd;

                list_append(&partition_list, &hd->logic_patrs[l_no].part_tag);
                sprintf(hd->logic_patrs[l_no].name, \
                        "%s%d", hd->name, l_no + 5);//逻辑分区数字从5开始,主分区是1~4

                l_no++;
                if(l_no >= 8) return ;              //只支持8个逻辑分区,避免数组越界
            }
        }
        p++;
    }
    sys_free(bs);
}
/*打印分区信息*/
static bool partition_info(struct list_elem* pelem, int arg UNUSED){

    struct partition* part = elem2entry(struct partition, part_tag, pelem);
    printk("%s start_lba:0x%x sec_cnt:0x%x\n", \
           part->name, part->start_lba, part->sec_cnt);

    /*在此处return false 与函数本身功能无关,只是为了让主调函数list_traversal继续向下遍历元素*/
    return false;
}


//硬盘数据结构初始化
void ide_init() {

    printk("ide_init start\n");

    uint8_t hd_cnt = *((uint8_t*)(0x475));  //获取硬盘的数量,在地址0x475处获取硬盘的数量，将其存入变量hd_cnt中
                                            //低端1MB以内的虚拟地址和物理地址相同,所以虚拟地址0x457可以正确访问到物理地址0x457
    ASSERT(hd_cnt > 0);
    
    list_init(&partition_list);
    channel_cnt = DIV_ROUND_UP(hd_cnt, 2);  //一个ide通道上有两个硬盘，根据硬盘数量反推有几个ide通道

    struct ide_channel* channel;
    uint8_t channel_no = 0, dev_no = 0; 
    
    printk("channel_cnt = %d\n", channel_cnt);
    //处理每个通道上的硬盘,因为有两块硬盘,所以只有一个通道 channel_cnt = 1
    while(channel_no < channel_cnt) {

        channel = &channels[channel_no];
        sprintf(channel->name, "ide%d", channel_no);

        //为每个ide通道初始化端口基址及中断向量
        switch(channel_no) {

        case 0:
            channel->port_base = 0x1f0;     //ide0通道的起始端口号是0x1f0
            channel->irq_no    = 0x20 + 14; //从片8259A上倒数第二的中断引脚,中断号
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
         * 中断处理程序将此信号量sema_up，唤醒线程*/
        sema_init(&channel->disk_done, 0);
        register_handler(channel->irq_no, intr_hd_handler);
        
        /*分别获取两个硬盘的参数及分区信息*/
        while(dev_no < 2) {

            struct disk* hd = &channel->devices[dev_no];
            hd->my_channel = channel;
            hd->dev_no = dev_no;
            sprintf(hd->name, "sd%c", 'a' + channel_no * 2 + dev_no);
            identify_disk(hd);              //获取硬盘参数
            if(dev_no != 0) {               //内核本省的裸硬盘(hd60m) 不处理

                partition_scan(hd, 0);      //扫描该硬盘上的分区
            }
            p_no = 0, l_no = 0;
            dev_no++;
        }
        dev_no = 0;
        channel_no++;                       //下一个channel 
    }
    printk("\n all partition info\n");

    list_traversal(&partition_list, partition_info, (int)NULL);
    printk("ide_init done\n");
}

/*硬盘中断处理程序*/
void intr_hd_handler(uint8_t irq_no) {

    ASSERT(irq_no == 0x2e || irq_no == 0x2f);

    uint8_t ch_no = irq_no - 0x2e;
    struct ide_channel* channel = &channels[ch_no];

    ASSERT(channel->irq_no == irq_no);

    /*不必担心此中断是否对应的是这一次的expecting_intr, 每次读写硬盘时会申请锁,从而保证了同步一致性 */
    if(channel->expecting_intr) {

        channel->expecting_intr = false;
        sema_up(&channel->disk_done);

        /*读取状态寄存器使硬盘控制器认为此次的中断已被处理,从而硬盘可以继续执行新的读写*/
        inb(reg_status(channel));
    }
}




