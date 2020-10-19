///////////////////////////////////////////////////////////////
// File Name: bitmap.c
// Author: tt
// Email: tttt@xiyoulinux.org
// Created Time : 2020-10-09 13:57:42
// Description:
///////////////////////////////////////////////////////////////

#include "bitmap.h"
#include "stdint.h"
#include "string.h"
#include "print.h"
#include "interrupt.h"
#include "debug.h"

//将位图初始化
void bitmap_init(struct bitmap* btmp) {

    memset(btmp->bits, 0, btmp->btmp_bytes_len);
}

//判断bit_idx 位是否为1,若为1,则返回true, 否则返回false
bool bitmap_scan_test(struct bitmap* btmp, uint32_t bit_idx) {

    uint32_t byte_idx = bit_idx / 8;            //向下取整用于索引数组下标
    uint32_t bit_odd = bit_idx % 8;             //取余用于索引数组内的位
    
    return (btmp->bits[byte_idx] & (BITMAP_MASK << bit_odd));
}

//在位图中申请连续cnt个位,成功,则返回起始为下标,失败,返回1 
//成功返回这一个位　的下标
int bitmap_scan(struct bitmap* btmp, uint32_t cnt) {

    uint32_t idx_byte = 0;              //用于记录第一个空闲位所在的字节的索引
    //先逐字节比较, 
    //如果某一个字节值 为0xff, 则代表这个字节所有位都为1,如果这个字节中的所有位都为１
    //则开始判断下一个字节，直到有一个字节中出现了没有使用过的资源
    while((0xff == btmp->bits[idx_byte]) && (idx_byte < btmp->btmp_bytes_len)){

        idx_byte++;
    }

    ASSERT(idx_byte < btmp->btmp_bytes_len);

    if(idx_byte == btmp->btmp_bytes_len) {          //若该内存池找不到可用空间

        return -1;
    }

    //若在位图数组范围内的某字节内找到了空闲位,在该字节内逐位对比,返回空闲位的索引
    int idx_bit = 0;

    //和btmp->bits[idx_byte]这个字节逐位对比
    while((uint8_t)(BITMAP_MASK << idx_bit) & btmp->bits[idx_byte]){

        idx_bit++;
    }

    int bit_idx_start = idx_byte*8 + idx_bit;           //空闲位在位图内的下标
    if (cnt == 1) {

        return bit_idx_start;
    }

    uint32_t bit_left = (btmp->btmp_bytes_len*8 - bit_idx_start);
    //记录还有多少位可以判断
    uint32_t next_bit = bit_idx_start + 1;
    uint32_t count = 1;                         //用于记录找到的空闲位的个数 

    bit_idx_start = -1;
    while(bit_left-- > 0) {
        
        if(!(bitmap_scan_test(btmp, next_bit))) {       //若next_bit为0 
            count++;
        } else {
            count = 0;
        }
        if(count == cnt) {           //若找到连续cnt个空位 

            bit_idx_start = next_bit - cnt + 1;
            break;
        }
        next_bit++;
    }

    return bit_idx_start;
}

//将位图btmp的bit_idx位设置为value 
void bitmap_set(struct bitmap* btmp, uint32_t bit_idx, int8_t value){

    ASSERT((value == 0) || (value == 1));

    uint32_t byte_idx = bit_idx / 8;                //向下取整用于索引数组下标
    uint32_t bit_odd = bit_idx % 8;                //取余用于索引数组内的位 

    //一般会用0x1这样的数字对字节中的位操作
    //将1任意移动后再取反,或者先取反再移位,可用来对位置0操作
    if(value) {
        btmp->bits[byte_idx] |= (BITMAP_MASK << bit_odd);
    }else {

        btmp->bits[byte_idx] &= ~(BITMAP_MASK << bit_odd);
    }
}

