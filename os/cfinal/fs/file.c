///////////////////////////////////////////////////////////////
// File Name: fs/file.c
// Author: tt
// Email: tttt@xiyoulinux.org
// Created Time : 2020-10-28 17:04:55
// Description:
///////////////////////////////////////////////////////////////

#include "file.h"
#include "../lib/kernel/stdio-kernel.h"
#include "../kernel/global.h"
#include "../thread/thread.h"
#include "../device/ide.h"
#include "super_block.h"
#include "fs.h"

#define DEFAULT_SECS 1

/*文件表,文件结构数组,最多可同时打开MAX_FILE_OPEN次文件*/
struct file file_table[MAX_FILE_OPEN];

/*从文件表file_table 中获取一个空闲位,成功返回下标,失败返回 -1*/
int32_t get_free_slot_in_global(void) {

    uint32_t fd_idx = 3;                        //0,1,2已经被占
    while(fd_idx < MAX_FILE_OPEN) {

        if(file_table[fd_idx].fd_inode == NULL) break;          //如果这个inode结点没有被使用
        fd_idx++;
    }

    if(fd_idx == MAX_FILE_OPEN) {

        printk("exceed max open files\n");
        return -1;
    }

    return fd_idx;
}


/*将全局描述符下标安装到进程或线程自己的文件描述符组fd_table 中,成功返回下标,失败返回 -1*/
int32_t pcb_fd_install(int32_t global_fd_idx) {

    struct task_struct* cur = running_thread();
    uint8_t local_fd_idx = 3;                           //跨过stdin,stdout,stderr 

    while(local_fd_idx < MAX_FILES_OPEN_PER_PROC) {

        if(cur->fd_table[local_fd_idx] == -1) {         //-1表示free_slot,可用

            cur->fd_table[local_fd_idx] = global_fd_idx;
            break;
        }
        local_fd_idx++;
    }

    if(local_fd_idx == MAX_FILES_OPEN_PER_PROC) {

        printk("exceed max open file_per_proc\n");
        
        return -1;
    }

    return local_fd_idx;
}

/*分配一个i节点,返回i结点号*/
int32_t inode_bitmap_alloc(struct partition* part) {

    int32_t bit_idx = bitmap_scan(&part->inode_bitmap, 1);
    if(bit_idx == -1) return -1;

    bitmap_set(&part->inode_bitmap, bit_idx, 1);

    return bit_idx;
}

/*分配一个扇区,返回其扇区地址*/
int32_t block_bitmap_alloc(struct partition* part) {

    int32_t bit_idx = bitmap_scan(&part->block_bitmap, 1);
    if(bit_idx == -1)  return -1;

    bitmap_set(&part->block_bitmap, bit_idx, 1);
    /*和inode_bitmap_malloc 不同,此处返回的不是位图索引,
     * 而是具体可用的扇区地址
     */

    return (part->sb->data_start_lba + bit_idx);
}

/*将内存中bitmap第bit_idx位所在的512字节同步到硬盘上 */
void bitmap_sync(struct partition* part, uint32_t bit_idx, uint8_t btmp) {

    uint32_t off_sec = bit_idx / 4096;                      //本i结点索引相对于位图的扇区偏移量
    uint32_t off_size = off_sec * BLOCK_SIZE;               //本i结点索引相对于位图的字节偏移量

    uint32_t sec_lba;
    uint8_t* bitmap_off;

    /*需要被同步到硬盘的位图只有inode_bitmap 和 block_bitmap */
    switch(btmp) {

    case INODE_BITMAP:
        sec_lba = part->sb->inode_bitmap_lba + off_sec;
        bitmap_off = part->inode_bitmap.bits + off_size;

        break;

    case BLOCK_BITMAP:
        sec_lba = part->sb->block_bitmap_lba + off_size;
        bitmap_off = part->block_bitmap.bits + off_size;

        break;
    }

    ide_write(part->my_disk, sec_lba, bitmap_off, 1);
}




