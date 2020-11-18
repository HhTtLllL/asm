#ifndef __FS_SUPER_BLOCK_H
#define __FS_SUPER_BLOCK_H 

#include "stdint.h"

/*超级块 ---每一个分区一个超级块*/
struct super_block {

    uint32_t magic;                 //魔数
    uint32_t sec_cnt;               //分区的总扇区数     --这里一个扇区等于一个块
    uint32_t inode_cnt;             //本分区中inode的数量
    uint32_t part_lba_base;         //本分区的起始lba地址

    uint32_t block_bitmap_lba;      //块位图本身起始扇区地址
    uint32_t block_bitmap_sects;    //扇区位图本身占用的扇区数量

    uint32_t inode_bitmap_lba;      //i 节点位图起始扇区lba地址
    uint32_t inode_bitmap_sects;    //i 节点位图占用的扇区数量

    uint32_t inode_table_lba;       //i 结点表起始扇区lba地址
    uint32_t inode_table_sects;     //i 结点表占用的扇区数量

    uint32_t data_start_lba;        //数据区开始的第一个扇区号   - 空闲块起始地址
    uint32_t root_inode_no;         //根目录所在的i结点号
    uint32_t dir_entry_size;        //目录项大小

    uint8_t pad[460];               //加上460字节,凑够 512 字节1扇区大小
}__attribute__ ((packed));

#endif
