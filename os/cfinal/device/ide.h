#ifndef __DEVICE_IDE_H
#define __DEVICE_IDE_H 

#include "stdint.h"
#include "../thread/sync.h"
#include "../lib/kernel/bitmap.h"
#include "../lib/kernel/list.h"

//分区结构
struct partition {

    uint32_t start_lba;                 //起始扇区
    uint32_t sec_cnt;                   //扇区数,这个分区可容纳分区的数量
    struct disk* my_disk;               //分区所属的硬盘,一个硬盘有很多分区，所以此成员my_disk用来表示此扇区属于那个硬盘
    struct list_elem part_tag;          //用于队列中的标记,将来会将分区汇总到队列中，需要使用此标记

    char name[8];                       //分区名称,如sda1,sda2
    struct super_block* sb;             //本分区的超级块
    struct bitmap block_bitmap;         //块位图,用来管理本分区的所有块,块的大小是由1个扇区组成的,所以block_bitmap就是管理扇区的位图
    struct bitmap inode_bitmap;         //i 节点位图, i结点管理位图
    struct list open_inodes;            //本分区打开的i节点队列,分区所打开的inode 队列
};



//硬盘结构
struct disk {

    char name[8];                       //本硬盘的名称，如sda 

    struct ide_channel* my_channel;     //此块硬盘归属于那个ide通道,一个通道上有两块硬盘，所以此成员用于指定本硬盘所属的通道

    uint8_t dev_no;                     //本硬盘是主0，还是从 1

    struct partition prim_parts[4];     //主分区顶多是４个
    struct partition logic_patrs[8];    //逻辑分区数量无限，但需要有个上限，支持８个
};


//ata 通道结构,也就是ide通道
struct ide_channel {

    char name[8];                       //本ata通道名称

    uint16_t port_base;                 //本通道的起始端口号
    uint8_t irq_no;                     //本通道所用的中断号,在硬盘的中断处理程序中要根据中断号来判断在哪个通道中操作

    struct lock lock;                   //通道锁
    bool expecting_intr;                //表示等待硬盘的中断,表示本通道正在等待硬盘中断
    
    /*驱动程序向硬盘发送命令后，在等待硬盘工作期间可以通过此信号量阻塞自己，避免干等着浪费CPU,等因硬盘工作完成后会发出中断，\
     * 中断处理程序通过此信号量将因硬盘驱动程序唤醒 */
    struct semaphore disk_done;         //用于阻塞、唤醒驱动程序

    struct disk devices[2];             //一个通道上连接两个硬盘，一个主盘一个从盘
};

void ide_init(void);

extern uint8_t channel_cnt;
extern struct ide_channel channels[];
extern struct list partition_list;

void intr_hd_handler(uint8_t irq_no);
void ide_read(struct disk* hd, uint32_t lba, void* buf, uint32_t sec_cnt);
void ide_write(struct disk* hd, uint32_t lba, void* buf, uint32_t sec_cnt);

#endif
