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
#include "inode.h"
#include "dir.h"
#include "../lib/string.h"
#include "../kernel/interrupt.h"
#include "../kernel/debug.h"


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
        sec_lba = part->sb->block_bitmap_lba + off_sec;
        bitmap_off = part->block_bitmap.bits + off_size;

        break;
    }

    ide_write(part->my_disk, sec_lba, bitmap_off, 1);
}

/*创建文件,若成功则返回文件描述符,否则返回-1
 *
 * 在目录parent_dir 中以模式flag 去创建普通文件filename,成功则返回文件描述符
 * */
int32_t file_create(struct dir* parent_dir, char* filename, uint8_t flag) {

    /*后续操作的公共缓冲区*/
    void* io_buf = sys_malloc(1024);
    if(NULL == io_buf) {

        printk("in file_creat: sys_malloc for io_buf failed\n");
        return -1;
    }

    uint8_t rollback_step = 0;                                          //用于操作失败时回滚各种资源状态

    /*为新文件分配 inode*/
    int32_t inode_no = inode_bitmap_alloc(cur_part);
    if(-1 == inode_no) {

        printk("in file_creat: allocate inode faild\n");
        return -1;
    }


    /*此inode要存堆中申请内存, 不可以生成局部变量, 因为file_table数组中的文件描述符的inode指针要指向它*/
    struct inode* new_file_inode = (struct inode*)sys_malloc(sizeof(struct inode));
    if(NULL == new_file_inode) {

        printk("file_create: sys_malloc for inode failed\n");
        rollback_step = 1;
        goto rollback;
    }
    inode_init(inode_no, new_file_inode);

    /*返回的是file_table 的下标*/
    int fd_idx = get_free_slot_in_global();
    if(-1 == fd_idx) {

        printk("exceed max open files\n");
        rollback_step = 2;
        goto rollback;
    }

    file_table[fd_idx].fd_inode = new_file_inode;
    file_table[fd_idx].fd_pos = 0;
    file_table[fd_idx].fd_flag = flag;
    file_table[fd_idx].fd_inode->write_deny = false;

    struct dir_entry new_dir_entry;
    memset(&new_dir_entry, 0, sizeof(struct dir_entry));

    create_dir_entry(filename, inode_no, FT_REGULAR, &new_dir_entry);
    //create_dir_entry 只是内存操作不出以外,不会返回失败
    
    /*同步内存数据到硬盘*/
    /*a 在目录parent_dir 下安装目录new_dir_entry,写入硬盘后返回true, 否则false*/
    if(!sync_dir_entry(parent_dir, &new_dir_entry, io_buf)) {

        printk("sync dir_entry to disk filed\n");
        rollback_step = 3;
        goto rollback;
    }
    
    memset(io_buf, 0, 1024);                                      
    /*b 将父目录 i 结点的内容同步到硬盘*/
    inode_sync(cur_part, parent_dir->inode, io_buf);
    
    memset(io_buf, 0, 1024);
    /*c 将新创建的文件的 i节点内容同步到硬盘上*/
    inode_sync(cur_part, new_file_inode, io_buf);

    /*d 将inode_bitmap位图同步到硬盘*/
    bitmap_sync(cur_part, inode_no, INODE_BITMAP);

    /*e 将创建的文件添加到 open_inodes 链表*/
    list_push(&cur_part->open_inodes, &new_file_inode->inode_tag);
    new_file_inode->i_open_cnts = 1;

    sys_free(io_buf);

    return pcb_fd_install(fd_idx);
    

/*创建文件需要创建相关的多个资源,若某一步失败则会执行到下面的回滚步骤*/
rollback:
    switch(rollback_step) {

    case 3:
        /*失败时,将file_table 中的相应位清空*/
        memset(&file_table[fd_idx], 0, sizeof(struct file));
    case 2:
        sys_free(new_file_inode);
    
    case 1:
        /*如果新文件的i 节点创建失败,之前位图中分配的 inode_no 也要恢复*/
        bitmap_set(&cur_part->inode_bitmap, inode_no, 0);
        break;
    }
    sys_free(io_buf);

    return -1;
}



/*打开编号为inode_no 的对应的文件,若成功则返回文件描述符,否则返回 -1 
 *
 * flag 打开标识,成功返回文件描述符
 * */
int32_t file_open(uint32_t inode_no, uint8_t flag) {

    int fd_idx = get_free_slot_in_global();         //从文件表获取一个空闲位 -- 文件描述符
    if(-1 == fd_idx) {

        printk("exceed max open files\n");
        return -1;
    }

    //在这个
    file_table[fd_idx].fd_inode = inode_open(cur_part, inode_no); 
    file_table[fd_idx].fd_pos = 0;

    //每次打开文件,要将fd_pos还原为0, 即让文件内的指针指向开头
    file_table[fd_idx].fd_flag = flag;
    bool* write_deny =(bool*)&file_table[fd_idx].fd_inode->write_deny;

    if(flag & O_WRONLY || flag & O_RDWR) {

        //只要是关于写文件,判断是否有其他进程正在写此文件,若是读文件,不考虑write_deny 
        //以下进入临界区前先关闭中断
        enum intr_status old_status = intr_disable();
        if(!(*write_deny)) {

            *write_deny = true;                             //置为true,避免多个进程同时写此文件
            intr_set_status(old_status);                    //恢复中断
        }else {

            intr_set_status(old_status);
            printk("file can't be write now, try again later\n");

            return -1;
        }
    }

    //若是读文件或者创建文件,不用理会write_deny,保持默认 
    return pcb_fd_install(fd_idx);
}

/*关闭文件*/
int32_t file_close(struct file* file) {

    if( NULL == file ) {

        return -1;
    }

    file->fd_inode->write_deny = false;
    inode_close(file->fd_inode);
    file->fd_inode = NULL;                                  //是文件结构可用

    return 0;
}

/*把 buf 中count个字节写入 file, 成功则把返回写入的字节数,失败则返回 -1*/
int32_t file_write(struct file* file, const void* buf, uint32_t count) {

    if((file->fd_inode->i_size + count) > (BLOCK_SIZE * 140)) {

        //文件目前最大支持 512 * 140 = 71680字节
        printk("exceed max filke_size 71680 字节, write file failed\n");
        return -1;
    }

    uint8_t* io_buf = sys_malloc(512);
    if( NULL == io_buf ) {
    
        printk("file_write: sys_malloc for io_buf failed\n");
        return -1;
    }

    uint32_t* all_blocks = (uint32_t*)sys_malloc(BLOCK_SIZE + 48);
    if( NULL == all_blocks ) {

        printk("file_write: sys_malloc for all_blocks failed\n");
        return -1;
    }

    const uint8_t* src = buf;                                   //用src 指向buf中待写入的数据
    uint32_t bytes_written = 0;                                 //用来记录已经写入数据大小
    uint32_t size_left = count;                                 //用来记录未写入数据大小

    int32_t block_lba = -1;                                     //块地址
    uint32_t block_bitmap_idx = 0;                              //用来记录block对应与block_bitmap中的索引,作为参数传给bitmao_sync 

    uint32_t sec_idx;                                           //用来索引扇区
    uint32_t sec_lba;                                           //扇区地址
    uint32_t sec_off_bytes;                                     //扇区内字节偏移量
    uint32_t sec_left_bytes;                                    //扇区内剩余字节量
    uint32_t chunk_size;                                        //每次写入硬盘的数据块大小
    int32_t indirect_block_table;                               //用来获取一级间接表地址
    uint32_t block_idx;                                         //块索引

    //判断文件是否第一次写,如果是先为其分配一个块
    if(file->fd_inode->i_sectors[0] == 0) {

        block_lba = block_bitmap_alloc(cur_part);
        if(-1 == block_lba) {

            printk("file_write:block_bitmap_alloc failed\n");
            return -1;
        }

        file->fd_inode->i_sectors[0]  = block_lba;

        /*每分配一个块就将位图同步到硬盘*/
        block_bitmap_idx = block_lba - cur_part->sb->data_start_lba;
        ASSERT(block_bitmap_idx != 0);
        bitmap_sync(cur_part, block_bitmap_idx, BLOCK_BITMAP);
    }

    /*写入count个字节前,该文件已经占用的块数*/
    uint32_t file_has_used_blocks = file->fd_inode->i_size / BLOCK_SIZE + 1;

    /*存储count个字节后,该文件将占用的块数*/
    uint32_t file_will_use_blocks = (file->fd_inode->i_size + count) / BLOCK_SIZE + 1;
    ASSERT(file_will_use_blocks <= 140);

    /*通过增量判断是否需要分配扇区,如果增量为0,则表示原扇区够用*/
    uint32_t add_blocks = file_will_use_blocks - file_has_used_blocks;
    
    /*将写文件所用到的块地址收集到all_blocks,系统中块大小等于扇区大小,后面统一在all_blocks中获取写入扇区地址*/
    if(0 == all_blocks) {

        /*在同一扇区内写入数据,不设计分配新扇区*/
        if(file_will_use_blocks <= 12) {                    //文件数据量在12块之内

            block_idx = file_has_used_blocks - 1;
            //指向最后一个已有数据的扇区
            all_blocks[block_idx] = file->fd_inode->i_sectors[block_idx];
        }else {

            //未写入新数据之前就已经占用间接快,需要将间接块地址读进来
            ASSERT(file->fd_inode->i_sectors[12] != 0); 
            indirect_block_table = file->fd_inode->i_sectors[12];
            ide_read(cur_part->my_disk, indirect_block_table, all_blocks + 12, 1);
        }
    }else {

        //若有增量,便涉及分配新扇区及是否分配一级间接块表,
    }







}

