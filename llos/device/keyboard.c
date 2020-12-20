///////////////////////////////////////////////////////////////
// File Name: keyboard.c
// Author: tt
// Email: tttt@xiyoulinux.org
// Created Time : 2020-10-13 18:29:03
// Description:
///////////////////////////////////////////////////////////////

#include "keyboard.h"
#include "../lib/kernel/print.h"
#include "../kernel/interrupt.h"
#include "../lib/kernel/io.h"
#include "../kernel/global.h"
#include <stdbool.h>
#include "ioqueue.h"

#define KBD_BUF_PORT 0x60           //键盘buffer寄存器端口号为0x60

//用转移字符定义部分控制字符
#define esc          '\033'         //八进制表示字符，也可以用16进制 '\x1b'
#define backspace    '\b'
#define tab          '\t'
#define enter        '\r'
#define delete       '\177'         //八进制表示字符，十六进制为 '\x7f'


//以上不可见字符一律定义为 0
#define char_invisible   0
#define ctrl_l_char     char_invisible 
#define ctrl_r_char     char_invisible
#define shift_l_char    char_invisible 
#define shift_r_char    char_invisible
#define alt_l_char      char_invisible
#define alt_r_char      char_invisible 
#define caps_lock_char  char_invisible 


//定义控制字符的通码和断码 
#define shift_l_make    0x2a
#define shift_r_make    0x36
#define alt_l_make      0x38
#define alt_r_make      0xe038
#define alt_r_break     0xe0b8
#define ctrl_l_make     0x1d
#define ctrl_r_make     0xe01d 
#define ctrl_r_break    0xe09d 
#define caps_lock_make  0x3a 

struct ioqueue kbd_buf;                 //定义键盘缓冲区

//定义以下变量记录相应键是否按下的状态， ext_scancode 用于记录makecode 是否以0x30开头
static bool ctrl_status, shift_status, alt_status, caps_lock_status, ext_scancode;

//以通码make_code 为索引的二位数组
static char keymap[][2] = {

    //扫描码未与shift组合 
    /* 0x00 */      {0, 0},
    /* 0x01 */      {esc, esc},
    /* 0x00 */      {'1', '!'},
    /* 0x00 */      {'2', '@'},
    /* 0x00 */      {'3', '#'},
    /* 0x00 */      {'4', '$'},
    /* 0x00 */      {'5', '%'},
    /* 0x00 */      {'6', '^'},
    /* 0x00 */      {'7', '&'},
    /* 0x00 */      {'8', '*'},
    /* 0x00 */      {'9', '('},
    /* 0x00 */      {'0', ')'},
    /* 0x00 */      {'-', '_'},
    /* 0x00 */      {'=', '+'},
    /* 0x00 */      {backspace, backspace},
    /* 0x00 */      {tab, tab},
    /* 0x00 */      {'q', 'Q'},
    /* 0x00 */      {'w', 'W'},
    /* 0x00 */      {'e', 'E'},
    /* 0x00 */      {'r', 'R'},
    /* 0x00 */      {'t', 'T'},
    /* 0x00 */      {'y', 'Y'},
    /* 0x00 */      {'u', 'U'},
    /* 0x00 */      {'i', 'I'},
    /* 0x00 */      {'o', 'O'},
    /* 0x00 */      {'p', 'P'},
    /* 0x00 */      {'[', '{'},
    /* 0x00 */      {']', '}'},
    /* 0x00 */      {enter, enter},
    /* 0x00 */      {ctrl_l_char, ctrl_l_char},
    /* 0x00 */      {'a', 'A'},
    /* 0x00 */      {'s', 'S'},
    /* 0x00 */      {'d', 'D'},
    /* 0x00 */      {'f', 'F'},
    /* 0x00 */      {'g', 'G'},
    /* 0x00 */      {'h', 'H'},
    /* 0x00 */      {'j', 'J'},
    /* 0x00 */      {'k', 'K'},
    /* 0x00 */      {'l', 'L'},
    /* 0x00 */      {';', ':'},
    /* 0x28 */	    {'\'','"'},	
    /* 0x00 */      {'`', '~'},
    /* 0x00 */      {shift_l_char, shift_l_char},
    /* 0x00 */      {'\\', '|'},
    /* 0x00 */      {'z', 'Z'},
    /* 0x00 */      {'x', 'X'},
    /* 0x00 */      {'c', 'C'},
    /* 0x00 */      {'v', 'V'},
    /* 0x00 */      {'b', 'B'},
    /* 0x00 */      {'n', 'N'},
    /* 0x00 */      {'m', 'M'},
    /* 0x00 */      {',', '<'},
    /* 0x00 */      {'.', '>'},
    /* 0x00 */      {'/', '?'},
    /* 0x00 */      {shift_r_char, shift_r_char},
    /* 0x00 */      {'*', '*'},
    /* 0x00 */      {alt_l_char, alt_l_char},
    /* 0x00 */      {' ', ' '},
    /* 0x00 */      {caps_lock_char, caps_lock_char},
};

//键盘中断处理程序
static void intr_keyboard_handler(void) {

    //必须要读取输出缓冲区寄存器，否则8042不在相应键盘中断
    //这次中断发生前的上一次中断，以下任意三个键是否有按下
    bool ctrl_down_last  = ctrl_status;
    bool shift_down_last = shift_status;
    bool caps_lock_last  = caps_lock_status;

    bool break_code;
    uint16_t scancode = inb(KBD_BUF_PORT);             //获取扫描码

    //若扫描码scancode 是以e0开头的，表示此键的按下将产生多个扫描码
    //所以马上结束此次中断处理函数，等待下一个扫描码进来
    if (scancode == 0xe0) {
            
        ext_scancode = true;                            //打开e0 标记
     
        return ;
    }

    //如果上次是以0xe0开头的，将扫描码合并
    if (ext_scancode) {

        scancode = ((0xe000) | scancode);
        ext_scancode = false;                           //关闭e0标记
    }
    
    //判断这个扫描码是否为断码， 断码的第8位为1
    break_code = ((scancode & 0x0080) != 0);            //获取break_code 
    if(break_code){
        //由于ctrl_r 和 alt_r 的make_code和break_code都是两字节 
        //所以可用下面的方法获取make_code,多字节的扫描码暂不处理 
        uint16_t make_code = (scancode &= 0xff7f);                  //将其还原为通码，
        //得到其make_code (按键按下时产生的扫描码)
        //若是任意以下三个键弹起了，将状态设置为false 
        if( make_code == ctrl_l_make || make_code == ctrl_r_make ) {

            ctrl_status = false;
        }else if ( make_code == shift_l_make || make_code == shift_r_make ) {

            shift_status = false;
        }else if( make_code == alt_l_make || make_code == alt_r_make ){

            alt_status = false;
        }   //由于caps_lock 不是弹起后关闭，所以需要单独处理
        
        return ;            //直接返回此次中断处理程序
    }
    //若为通码,只处理数组中定义的键一级alt_right和ctrl键，全是make_code 
    else if ((scancode > 0x00 && scancode < 0x3b) || \
             (scancode == alt_r_make) || \
             (scancode == ctrl_r_make)) {

        bool shift = false;     //判断是否与shift组合，用来在一位数组中索引对应的字符
        if((scancode < 0x0e) || (scancode == 0x29) || \
           (scancode == 0x1a) || (scancode == 0x1b) || \
           (scancode == 0x2b) || (scancode == 0x27) || \
           (scancode == 0x28) || (scancode == 0x33) || \
           (scancode == 0x34) || (scancode == 0x35)) {

            /*代表两个字幕的键
             0x0e 数字'0'～'9',字符'-',字符'='
             0x29 字符'`'
             0x1a 字符'['
             0x1b 字符']'
             0x2b 字符'\\'
             0x27 字符';'
             0x28 字符'\''
             0x33 字符','
             0x34 字符'.'
             0x35 字符'/'
             * */

            if (shift_down_last) {  //如果同时按下了shift键

                shift= true;
            }
        }else {         //默认为字母键
            if(shift_down_last && caps_lock_last) {     //如果shift 和capslock同时被按下,同时按下的话，抵消大写的功能

                shift = false;
            }else if(shift_down_last || caps_lock_last) {   //如果shift 和capslock任意被按下

                shift = true;
            }else {

                shift = false;
            }
        }

        uint8_t index = (scancode &= 0x00ff);
        //如果是单字节，可以用0xff ，但是还有双字节的，高位为x0e0，用00ff可以将0xe0抵消掉
        //将扫描码的高字节置0，主要针对高字节是e0的扫描码
        char cur_char = keymap[index][shift];       //在数组中找到对应的字符

        //如果cur_char 不为0，也就是ascii 码为除'\0'外的字符就加入键盘缓冲区中
        if (cur_char){

                 /*****************  快捷键ctrl+l和ctrl+u的处理 *********************
                  * 下面是把ctrl+l和ctrl+u这两种组合键产生的字符置为:
                  * cur_char的asc码-字符a的asc码, 此差值比较小,
                  * 属于asc码表中不可见的字符部分.故不会产生可见字符.
                  * 我们在shell中将ascii值为l-a和u-a的分别处理为清屏和删除输入的快捷键*/
            if ((ctrl_down_last && cur_char == 'l') || (ctrl_down_last && cur_char == 'u')) {
	    
                cur_char -= 'a';
            }

            /*若kbd_buf中未满并且待加入的cur_har不为0
             *则将其加入到缓冲区kbd_buf中
             * */
            if( !ioq_full(&kbd_buf) ) {

     //           put_char(cur_char);
                ioq_putchar(&kbd_buf, cur_char);
            }


            return ;
        }

        //记录恩赐是否按下了下面积累控制键之1，共下次键入时判断组合键
        if(scancode == ctrl_l_make || scancode == ctrl_r_make) {

            ctrl_status = true;
        }else if(scancode == shift_l_make || scancode == shift_r_make){

            shift_status = true;
        }else if(scancode == alt_l_make || scancode == alt_r_make) {

            alt_status = true;
        }else if(scancode == caps_lock_make) {

            //不管之前是否有按下caps_lock 键，当在此按下的时，则状态相反，
            //即开启时按下则关闭，关闭时按下则开启
            caps_lock_status = !caps_lock_status;
        }

    }else {

        put_str("unknow key\n");
    }
}

//键盘初始化
void keyboard_init() {

    put_str("keyboard init start\n");
    ioqueue_init(&kbd_buf);
    register_handler(0x21, intr_keyboard_handler);
    put_str("keyboard init done\n");
}


