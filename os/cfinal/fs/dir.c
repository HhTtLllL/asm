///////////////////////////////////////////////////////////////
// File Name: fs/dir.c
// Author: tt
// Email: tttt@xiyoulinux.org
// Created Time : 2020-10-28 21:28:37
// Description:
///////////////////////////////////////////////////////////////
#include "dir.h"
#include "fs.h"
#include "../device/ide.h"
#include "super_block.h"
#include "inode.h"
#include "../lib/kernel/stdio-kernel.h"
#include "../lib/string.h"
#include "../kernel/debug.h"

struct dir root_dir;                                            //根目录

/*打开根目录*/
void open_root_dir(struct partition* part) {
    
    root_dir.inode = inode_open(part, part->sb->root_inode_no); 
    root_dir.dir_pos = 0;
}

/*在分区part上打开 i 结点为inode_no的目录并返回目录指针*/
struct dir* dir_open(struct partition* part, uint32_t inode_no) {
 
    struct dir* pdir = (struct dir*)sys_malloc(sizeof(struct dir));
    pdir->inode = inode_open(part, inode_no);
    pdir->dir_pos = 0;

    return pdir;
}

/*在part分区内的pdir目录内寻找名为name的文件或目录,找到后返回true并将其目录项存入dir_e,否则返回false*/
bool search_dir_entry(struct partition* part, struct dir* pdir, const char* name, struct dir_entry* dir_e) {

    /*inode 最多由 12个直接块和 128个间接块 组成*/
    uint32_t block_cnt = 140;                               //12个直接块 + 128个一级间接块 = 140块
    
    /*12个直接块 +　128个间接块,共560个字节*/
    uint32_t* all_blocks = (uint32_t*)sys_malloc(48 + 512);
    if(all_blocks == NULL) {

        printk("search_dir_entry: ysy_malloc for all_blocks failed");
        
        return false;
    }

    uint32_t block_idx = 0;
    while(block_idx < 12) {

        all_blocks[block_idx] = pdir->inode->i_sectors[block_idx];
        block_idx++;
    }

    if(pdir->inode->i_sectors[12] != 0) {                               //若含有一级间接块表
        /*只要i_sectors[12] 不为0, 就表示有一级间接块索引*/
        ide_read(part->my_disk, pdir->inode->i_sectors[12], all_blocks + 12, 1);
    }
    /*至此,all_blocks 存储的是该文件或目录的所有扇区地址*/

    /*写目录项的时候已经保证不会跨扇区,这样读目录时容易处理,只申请容纳1个扇区的内存*/
    uint8_t* buf = (uint8_t*)sys_malloc(SECTOR_SIZE);
    struct dir_entry* p_de = (struct dir_entry*)buf;
    //p_de为指向目录项的指针,值为buf起始地址,用p_de来遍历 buf 
    
    uint32_t dir_entry_size = part->sb->dir_entry_size;
    uint32_t dir_entry_cnt = SECTOR_SIZE / dir_entry_size;                  //一个扇区内的目录项的个数
    
    block_idx = 0;                                                          //重新将索引值置0
    /*开始在所有块中查找目录项*/
    while(block_idx < block_cnt) {
        
        /*块地址为0时表示该块中无数据,继续在其他块中找*/
        if(all_blocks[block_idx] == 0) {
            block_idx++;
            continue;
        }
        ide_read(part->my_disk, all_blocks[block_idx], buf, 1);             //读入一扇区的数据

        uint32_t dir_entry_idx = 0;
        /*遍历扇区中的所有目录项*/
        while(dir_entry_idx < dir_entry_cnt) {                              //用p_de来遍历 buf中的所有项 

            /*若找到了,就直接复制整个目录项*/
            if(!strcmp(p_de->filename, name)) {

                memcpy(dir_e, p_de, dir_entry_size);
                sys_free(buf);
                sys_free(all_blocks);

                return true;
            }
            dir_entry_idx++;
            p_de++;
        }
        block_idx++;
        p_de = (struct dir_entry*)buf;                                  //此时p_de 已经指向扇区内最后一个完整的目录项, 需要恢复p_de指向为 buf 

        memset(buf, 0, SECTOR_SIZE);                                    //将buf 清0,下次再用
    }
    sys_free(buf);
    sys_free(all_blocks);

    return false;
}


/*关闭目录*/
void dir_close(struct dir* dir) {

    /*根目录不能关闭, 
     *1. 根目录打开之后就不应该关闭, 否则还需要在此 open_root_dir 
     *2. root_dir 所在内存是低端1Mb之内, 并非在堆中,free会出问题
     */
    if(dir == &root_dir) {

        return ;                                        //直接返回
    }
    inode_close(dir->inode);                            //关闭目录的inode 

    sys_free(dir);
}

/*在内存中初始化目录项p_de */
void create_dir_entry(char* filename, uint32_t inode_no, uint8_t file_type, struct dir_entry* p_de) {

    ASSERT(strlen(filename) <= MAX_FILE_NAME_LEN);

    /*初始化目录项*/
    memcpy(p_de->filename, filename, strlen(filename));
    p_de->i_no = inode_no;
    p_de->f_type = file_type;
}


