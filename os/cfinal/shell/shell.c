///////////////////////////////////////////////////////////////
// File Name: shell/shell.c
// Author: tt
// Email: tttt@xiyoulinux.org
// Created Time : 2020-11-04 15:07:07
// Description:
///////////////////////////////////////////////////////////////
#include "shell.h"
#include "../kernel/global.h"
#include "../lib/stdio.h"
#include "../lib/stdint.h"
#include "../lib/user/syscall.h"
#include "../fs/file.h"
#include "../lib/string.h"
#include "../kernel/debug.h"
#include "../lib/user/assert.h"
#include "buildin_cmd.h"

#define cmd_len     128                     //最大支持键入128个字符的命令行输入
#define MAX_ARG_NR  16                      //加上命令外, 最多支持15个参数

char* argv[MAX_ARG_NR];                     //argv必须为全局变量,为了以后exec的程序可以访问参数
int32_t argc = -1;
/*存储输入的命令*/
static char cmd_line[cmd_len] = {0};
char final_path[MAX_PATH_LEN] = {0};          //用于洗路径时的缓冲

/*用来记录当前目录, 是当前目录的缓存, 每次执行cd命令会更新此内容*/
char cwd_cache[MAX_PATH_LEN] = {0};

/*输出提示符*/
void print_prompt(void) {

    printf("[tt@tt %s]$", cwd_cache);
}

/*从键盘缓冲区中最多读入 count个字节到buf*/
static void readline(char* buf, int32_t count) {

    assert(NULL != buf && count > 0);
    char* pos = buf;

    while(read(stdin_no, pos, 1) != -1 && (pos - buf) < count) {
    
        //在不出错情况下,直到找到回车符才返回
        switch(*pos){

            /*找到回车或换行符后,认为键入的命令结束, 直接返回*/
        case '\n':
        case '\r':
            *pos = 0;                               //添加cmd_line的种植字符 0
            putchar('\n');
            
            return;

        case '\b': 
            if('\b' != buf[0]) {

                //阻止删除非本次输入的信息
                --pos;                              //回退到缓冲区cmd_line中上一个字符
                putchar('\b');
            }

            break;
        case 'l' - 'a':
            /*先将当前字符 'l' - 'a' 置为 0
             *然后清空屏幕
             打印提示符
             * */
            *pos = 0;
            clear();
            print_prompt();
            printf("%s", buf);

            break;
        case 'u' - 'a':

            while(buf != pos) {

                putchar('\b');
                *(pos--) = 0;
            }

            break;

            /*非控制字符则输出字符*/
        default:
            putchar(*pos);
            pos++;
        }
    }

    printf("readline: can't find enter_key in the cmd_line, max num of char is 128\n");
}

/*分析字符串cmd_str 中以token为分隔符的单词, 将各单词的指针存入argv数组*/
static int32_t cmd_parse(char* cmd_str, char** argv, char token) {

    assert(NULL != cmd_str);

    int32_t arg_idx = 0;
    while(arg_idx < MAX_ARG_NR) {

        argv[arg_idx] = NULL;
        arg_idx++;
    }

    char* next = cmd_str;
    int32_t argc = 0;
    /*外层循环处理整个命令行*/
    while(*next) {

        /*去除命令字或参数之间的空格*/
        while(*next == token) {
            
            next++;
        }

        /*处理最后一个参数后接空格的情况, */
        if(0 == *next) {

            break;
        }

        argv[argc] = next;
        /*内层循环处理命令行中的每个命令字及参数*/
        while(*next && *next != token) {                //在字符串结束前找单词分隔符

            next++;
        }

        /*如果未结束(是token 字符), 使tocken 变成0*/
        if(*next) {

            *next++ = 0;                                //将token字符替换为字符串结束符 0
                                                        //作为一个单词的结束, 并将字符指针next指向下一个字符
        }

        /*避免argv数组访问越界, 参数过多则返回 0*/
        if(argc > MAX_ARG_NR) {

            return -1;
        }
        argc++;
    }

    return argc;
}

/*简单的shell*/
void my_shell(void) {

    cwd_cache[0] = '/';
    //cwd_cache[1] = 0;
    while(1) {

        print_prompt();
        memset(final_path, 0, MAX_PATH_LEN);
        memset(cmd_line, 0, MAX_PATH_LEN);
        readline(cmd_line, MAX_PATH_LEN);
        if(cmd_line[0] == 0) {

            //若只键入一个回车
            continue;
        }

        argc = -1;
        argc = cmd_parse(cmd_line, argv, ' ');
        if(-1 == argc){

            printf("num of arguments exceed %d\n", MAX_ARG_NR);
            continue;
        }

        char buf[MAX_PATH_LEN] = {0};

        int32_t arg_idx = 0;
        if(!strcmp("ls", argv[0])) {

            buildin_ls(argc, argv);
        }else if(!strcmp("cd", argv[0])){

            if(buildin_cd(argc, argv) != NULL) {

                memset(cwd_cache, 0, MAX_PATH_LEN);
                strcpy(cwd_cache, final_path);
            }
        }else if(!strcmp("pwd", argv[0])) {

            buildin_pwd(argc, argv);
        }else if(!strcmp("ps", argv[0])) {

            buildin_ps(argc, argv);
        }else if(!strcmp("clear", argv[0])) {

            buildin_clear(argc, argv);
        }else if(!strcmp("mkdir", argv[0])) {

            buildin_clear(argc, argv);
        }else if(!strcmp("rmdir", argv[0])) {

            buildin_rmdir(argc, argv);
        }else if(!strcmp("rm", argv[0])) {

            buildin_rm(argc, argv);
        }else {

            printf("external commadn\n");
        }
    }

       /* while(arg_idx < argc) {
    
            make_clear_abs_path(argv[arg_idx], buf);
            printf("%s -> %s", argv[arg_idx], buf);
            arg_idx++;
        }

        printf("\n");
    }*/

    panic("my_shell: shoud not be here");
}
















