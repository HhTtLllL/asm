///////////////////////////////////////////////////////////////
// File Name: fs.c
// Author: tt
// Email: tttt@xiyoulinux.org
// Created Time : 2020-10-26 19:37:23
// Description:
///////////////////////////////////////////////////////////////
#include "fs.h"
#include "../device/ide.h"
#include "inode.h"
#include "super_block.h"
#include "dir.h"
#include "../lib/kernel/io.h"
#include "../lib/kernel/stdio-kernel.h"
#include "../lib/string.h"
#include "../kernel/debug.h"
#include "../kernel/memory.h"
#include "../kernel/global.h"
#include "../lib/kernel/list.h"
#include "stdint.h"

struct partition* cur_part;                                 //默认情况下操作的是那个分区

/*在分区链表中找到名为part_name 的分区,并将其指针赋值给cur_part*/
static bool mount_partition(struct list_elem* pelem, int arg) {

    char* part_name = (char*)arg;                           //获取分区名

    struct partition* part = elem2entry(struct partition, part_tag, pelem);
    if(!strcmp(part->name, part_name)) {

        cur_part = part;
        struct disk* hd = cur_part->my_disk;
        /*sb_buf 用来存储从硬盘上读入的超级块*/
        struct super_block* sb_buf = \
        (struct super_block*)sys_malloc(SECTOR_SIZE);

        /*在内存中创建分区cur_part的超级块*/
        cur_part->sb = (struct super_block*)sys_malloc(sizeof(struct super_block));
        if(cur_part->sb == NULL) {

            PANIC("alloc memory failed!");
        }
        /*读入超级块*/
        memset(sb_buf, 0, SECTOR_SIZE);
        ide_read(hd, cur_part->start_lba + 1, sb_buf, 1);

        /*把sb_buf 中超级块的信息赋值到超级块 sb 中*/
        memcpy(cur_part->sb, sb_buf, sizeof(struct super_block));

        /*                将硬盘上的块位图读入到内存中                     */
        cur_part->block_bitmap.bits = (uint8_t*)sys_malloc(sb_buf->block_bitmap_sects * SECTOR_SIZE);
        if(cur_part->block_bitmap.bits == NULL) {

            PANIC("alloc memory failed!");
        }

        cur_part->block_bitmap.btmp_bytes_len = sb_buf->block_bitmap_sects * SECTOR_SIZE;

        /*从硬盘上读入块位图到分区的block_bitmap_bits */
        ide_read(hd, sb_buf->block_bitmap_lba, cur_part->block_bitmap.bits, sb_buf->block_bitmap_sects);
        
        /*将硬盘上的inode位图存入到内存*/
        cur_part->inode_bitmap.bits = (uint8_t*)sys_malloc(sb_buf->inode_bitmap_sects * SECTOR_SIZE);
        if(cur_part->inode_bitmap.bits == NULL) {

            PANIC("alloc memory failed!");
        }

        cur_part->inode_bitmap.btmp_bytes_len = sb_buf->inode_bitmap_sects * SECTOR_SIZE;
        /*从硬盘上读入inode位图到分区的inode_bitmaps.bits*/
        ide_read(hd, sb_buf->inode_table_lba, cur_part->inode_bitmap.bits, sb_buf->inode_bitmap_sects);

        list_init(&cur_part->open_inodes);
        printk("mount %s done!\n", part->name);


        return true;
    }

    return false;
}

/*
 *参数: part 待创建文件系统的分区->part
 *功能: 格式化分区,也就是初始化分区的元信息,创建文件系统
 * */
static void partition_format(struct partition* part) {

    /* 一个块大小就是一个扇区 */
    uint32_t boot_sector_sects = 1;        //引导块
    uint32_t super_block_sects = 1;        //超级块
    uint32_t inode_bitmap_sects =          //inode位图占用的扇区数 --占一个扇区,512字节, *8,共支持4096个文件
         DIV_ROUND_UP(MAX_FILES_PER_PART, BITS_PER_SECTOR);
     //I 结点位图占用的扇区数,最多支持4096个文件
    uint32_t inode_table_sects = DIV_ROUND_UP(((sizeof(struct inode) * MAX_FILES_PER_PART)), SECTOR_SIZE);
    uint32_t used_sects = boot_sector_sects + super_block_sects + inode_bitmap_sects + inode_table_sects; 
    uint32_t free_sects = part->sec_cnt - used_sects;          //空闲块位图 + 空闲块数量
     /*****************简单处理块位图占据的扇区数 **************************/
    uint32_t block_bitmap_sects;                               //空闲块位图占用的扇区数
    block_bitmap_sects = DIV_ROUND_UP(free_sects, BITS_PER_SECTOR);

     /*block*/
    uint32_t block_bitmap_bit_len = free_sects - block_bitmap_sects;
    block_bitmap_sects = DIV_ROUND_UP(block_bitmap_bit_len, BITS_PER_SECTOR);       //空闲块最终占用的扇区数
    

    /*超级块 初始化*/
    struct super_block sb;
    sb.magic = 0x19590318;
    sb.sec_cnt = part->sec_cnt;                                         //分区的总扇区数
    sb.inode_cnt = MAX_FILES_PER_PART;                                  //本分区的inode的数量
    sb.part_lba_base = part->start_lba;                                 //本分区的起始lba地址
    
    sb.block_bitmap_lba = sb.part_lba_base + 2;                         //第0块是引导块,第 1 块是超级块
    sb.block_bitmap_sects = block_bitmap_sects;                         //扇区位图本身占用的扇区数量

    sb.inode_bitmap_lba = sb.block_bitmap_lba + sb.block_bitmap_sects;  //i结点位图起始扇区lba地址
    sb.inode_bitmap_sects = inode_bitmap_sects;                         //i结点占用的扇区数量
    
    sb.inode_table_lba = sb.inode_bitmap_lba + sb.inode_bitmap_sects;   //i结点表起始扇区lba地址
    sb.inode_table_sects = inode_table_sects;                           //i结点扇区数量

    sb.data_start_lba = sb.inode_table_lba + sb.inode_bitmap_sects;     //数据区开始的第一个扇区号
    sb.root_inode_no = 0;                                               //根目录所在的i结点号
    sb.dir_entry_size = sizeof(struct dir_entry);                       //目录项的大小

    printk("%s info:\n", part->name);
    printk("   magic:0x%x\n   part_lba_base:0x%x\n   all_sectors:0x%x\n \
           inode_cnt:0x%x\n   block_bitmap_lba:0x%x\n   block_bitmap_sectors:0x%x\n \
           inode_bitmap_lba:0x%x\n   inode_bitmap_sectors:0x%x\n   inode_table_lba:0x%x\n \
           inode_table_sectors:0x%x\n   data_start_lba:0x%x\n", sb.magic, sb.part_lba_base,\
           sb.sec_cnt, sb.inode_cnt, sb.block_bitmap_lba, sb.block_bitmap_sects, sb.inode_bitmap_lba,\
           sb.inode_bitmap_sects, sb.inode_table_lba, sb.inode_table_sects, sb.data_start_lba);


    struct disk* hd = part->my_disk;                                    //获取属于自己的硬盘,这个分区在那个硬盘上

    /*1 将超级块写入本分区的第一个扇区*/
    ide_write(hd, part->start_lba + 1, &sb, 1);                         //第0块是引导扇区的地方,所以这里跨过引导扇区

    /*找出数据量最大的元信息,用其尺寸做存储缓冲区*/
    uint32_t buf_size = (sb.block_bitmap_sects >= \
    sb.inode_bitmap_sects ? sb.block_bitmap_sects : sb.inode_bitmap_sects); 

    buf_size = (buf_size >= sb.inode_table_sects ? \
    buf_size : sb.inode_table_sects) * SECTOR_SIZE;
    
    //申请内存返回给指针buf
    uint8_t* buf = (uint8_t*)sys_malloc(buf_size);

    //申请的内存由内存管理系统清0后返回
    
    /*2 将位图初始化并写入sb.block_bitmap_lba
     *初始化块位图block_bitmap
     * */
    buf[0] |= 0x01;                                                     //第0个块预留给根目录,位图先占位,将空闲块位图中的第0为置1
    uint32_t block_bitmap_last_byte = block_bitmap_bit_len / 8;           
    uint8_t block_bitmap_last_bit = block_bitmap_bit_len % 8;           //多余的位
    //last_size 是位图所在最后一个扇区中,不足一扇区的其余部分
    uint32_t last_size = SECTOR_SIZE - (block_bitmap_last_byte % SECTOR_SIZE);

    /*1 先将位图最后1字节到其所在的扇区的结束全置为1,即超出实际块数的部分直接置为己占用*/
    memset(&buf[block_bitmap_last_byte], 0xff, last_size);

    /*2 再将上一步覆盖的最后一字节内的有效位重新置 0*/
    uint8_t bit_idx = 0;
    while(bit_idx <= block_bitmap_last_bit) {

        buf[block_bitmap_last_byte] &= ~(1 << bit_idx++);
    }

    ide_write(hd, sb.block_bitmap_lba, buf, sb.block_bitmap_sects);
    
    /*3 将inode位图初始化并写入sb.inode_bitmap_lba */
    //先清空缓冲区
    memset(buf, 0, buf_size);
    buf[0] |= 0x1;                                                  //第0个分给了根目录,将inode位图中第0个inode置为1
    /*由于 inode_table 中共4096个inode,位图inode_bitmap 正好占用1扇区,即inode_bitmap_sects 等于1 
     *所以位图中的位全都代表inode_table 中的 inode ,无需再像block_bitmap哪样单独处理最后一扇区的剩余部分
     *inode_bitmap所在的扇区中没有多余的无效位
     */
    ide_write(hd, sb.inode_bitmap_lba, buf, sb.inode_bitmap_sects);

    /*4 将inode数组初始化并写入 sb.inode_table_lba */
    /*准备写inode_table 中第 0 项,即根目录所在的inode */
    memset(buf, 0, buf_size);                               

    struct inode* i = (struct inode*)buf;
    i->i_size = sb.dir_entry_size * 2;                  //. 和 .. 
    i->i_no = 0;                                        //根目录栈inode数组中第0个inode 
    i->i_sectors[0] = sb.data_start_lba;                //由于上面的memset,i_sectors 数组中的其他元素都初始化为0

    ide_write(hd, sb.inode_table_lba, buf, sb.inode_table_sects);

    /*5 将根目录写入sb.data_start_lba
     *写入根目录的两个目录项 . 和 ..
     * */
    memset(buf, 0, buf_size);
    struct dir_entry* p_de = (struct dir_entry*)buf;

    /*初始化当前目录*/
    memcpy(p_de->filename, ".", 1);
    p_de->i_no = 0;
    p_de->f_type = FT_DIRECTORY;
    p_de++;                                             //p_de 指向下一个目录项

    //初始化当前父目录
    memcpy(p_de->filename, "..", 2);
    p_de->i_no = 0;                                     //根目录的父目录依然是根目录自己
    p_de->f_type = FT_DIRECTORY;
    
    /*sb.data_start_lba 已经分配给了根目录,里面是根目录的目录项*/
    ide_write(hd, sb.data_start_lba, buf, 1);

    printk(" root_dir_lba:0x%x\n", sb.data_start_lba);
    printk("%s format done\n", part->name);

    sys_free(buf);
}

/*将最上层的路径名称解析出来
 *1. 字符串形式的路径及文件名
 *2. 主调函数提供的缓冲区,用于存储最上层路径名
 *
 * 调用结束后,返回除顶层路径之外的自路径字符串的地址
 * 
 * */
static char* path_parse(char* pathname, char* name_store) {

    if(pathname[0] == '/') {                                //根目录不需要单独解析

        /*路径中出现1个或多个连续的字符 '/', 将这些'/'跳过,如: ///a/b */
        while(*(++pathname) == '/');
    }


    /*开始一般的路径解析*/
    while(*pathname != '/' && *pathname != 0) {

        *name_store++ = *pathname++;
    }

    if(pathname[0] == 0) {                                  //若路径字符串为空,则返回NULL

        return NULL;
    }

    return pathname;
}

/*返回路径深度, 比如: a/b/c 深度为3 */
int32_t path_depth_cnt(char* pathname) {

    ASSERT(pathname != NULL);

    char* p = pathname;
    char name[MAX_FILE_NAME_LEN];                           //用于path_parse的参数做路径解析

    uint32_t depth = 0;

    /*解析路径,从中拆分出各级名称*/
    p = path_parse(p, name);
    while(name[0]) {

        depth++;
        memcmp(name, 0, MAX_FILE_NAME_LEN);
        if(p) p = path_parse(p, name);
    }

    return depth;
}

/*在磁盘上搜索文件系统,若没有则格式化分区创建文件系统*/
void filesys_init() {

    uint8_t channel_no = 0, dev_no, part_idx = 0;

    /*sb_buf 用来存储从硬盘上读入的超级块*/
    struct super_block* sb_buf = \
    (struct super_block*)sys_malloc(SECTOR_SIZE);

    if(sb_buf == NULL) {

        PANIC("alloc memory failed!\n");
    }

    printk("searching filesystem...\n");
    while(channel_no < channel_cnt) {               //在分区上扫描文件系统,遍历通道

        dev_no = 0;
        while(dev_no < 2) {                         //遍历通道中的硬盘

            if(dev_no == 0) {                       //跨过裸盘tt.img 

                dev_no++;
                continue;
            }

            struct disk* hd = &channels[channel_no].devices[dev_no];
            struct partition* part = hd->prim_parts;
            while(part_idx < 12) {                  //4个主分区 + 8个逻辑 遍历硬盘上的分区

                if(part_idx == 4){                  //开始处理逻辑分区a

                    part = hd->logic_patrs;
                }
                /*channels 数组是全局变量,默认值为0,disk属于其嵌套结构,partition又为disk的嵌套结构,
                 *因此partitoon中的成员默认也为0, 若partition未初始化,则partition中的成员仍为 0
                 *下面处理存在的分区
                 */
                if(part->sec_cnt != 0) {            //如果分区存在

                    memset(sb_buf, 0, SECTOR_SIZE);
                    /*读出分区的超级块,根据魔术是否正确来判断是否存在文件系统*/
                    ide_read(hd, part->start_lba + 1, sb_buf, 1);
                    /*只支持自己的文件系统,若磁盘上已经有文件系统就不在格式化了*/
                    if(sb_buf->magic == 0x19590318) {

                        printk("%s has filesystem\n", part->name);
                    }else {                         //其他文件系统不支持,一律按无文件系统处理

                        printk("formatting %s sparition %s.....", hd->name, part->name);
                        partition_format(part);
                    }
                }
                part_idx++;
                part++;                             //下一分区
            }
            dev_no++;                               //下一磁盘
        }
        channel_no++;                               //下一通道
    }
    sys_free(sb_buf);
    
    /*确定默认操作的分区*/
    char default_part[8] = "sdb1";
    /*挂载分区*/
    list_traversal(&partition_list, mount_partition, (int)default_part);
}

