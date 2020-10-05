TI_GDT          equ 0 
RPL0            equ 0
SELECTOR_VIDEO  equ (0x0003 << 3) + TI_GDT + RPL0 

[bits 32]
section .text 
;---------------------------------------------
;put_str 铜鼓put_str 来打印以0字符结尾的字符串 
;----------------------------------------------
;输入:栈中参数为打印的字符串 
;输出:无

global put_str 
put_str:
;由于本函数中只用到了ebx 和 ecx, 只备份这两个寄存器 
    push ebx
    push ecx
    xor ecx, ecx                ;准备用ecx存储参数,清空
    mov esp, [esp + 12]          ;从栈中得到待打印的字符串地址

.goon:
    mov cl, [ebx]
    cmp cl, 0                   ;如果处理到了字符串尾，跳到结束处返回 
    jz .str_over 

    push ecx                    ;为put_char 函数传递参数 
    call put_char                
    add esp, 4                  ;回收参数所占的栈空间
    inc ebx                     ;使ebx指向下一个字符

    jmp .goon 

.str_over:
    
    pop ecx 
    pop ebx 
    ret 

;----------------- put_char --------------------------------------
;功能描述:把栈中的第一个字符写入光标所在处 ,直接写显存
;----------------------------------------------------------------
global put_char             ;导出为全局富豪,即外部文件也可以调用 
put_char:

    pushad                  ;备份32为寄存器环境  pushad == push all double 
                            ;需要保证 gs 中为正确的视频段选择子
                            ;为保险起见,每次打印时都为 gs 赋值 
    mov ax, SELECTOR_VIDEO  ;不能直接把立即数送入段寄存器 
    mov gs, ax 



;;;;;;;;;;;;;;; 获取光标位置 ;;;;;;;;;;;;;;;;; 

;先获得高 8 位 ,下面三行用来设置待操作寄存器的索引
    mov dx, 0x03d4                  ;索引寄存器 
    mov al, 0x0e                    ;用于提供光标位置的高 8 位  
    out dx, al                              
    
    mov dx, 0x03d5                  ;通过读写数据端口0x3d5 来获得或设置光标的位置
    in al, dx                       ;得到光标的高 8 位 
    mov ah, al   

    ;在获取低 8 位 
    mov dx, 0x03d4 
    mov al, 0x0f 
    out dx, al 
    mov dx, 0x03d5 
    in al, dx 

    
    ;将光标存入 bx 
    mov bx, ax 
    ;下面这行是在栈中获取待打印的字符
    mov ecx, [esp + 36]             ;pushad 压入 4x8 = 32 字节,加上主调函数4字节的返回地址,一共36字节 

    cmp cl, 0xd                     ;CR是0x0d, LF是0x0a 
    jz .is_carriage_return          ;判断是否为 CR 回车
    cmp cl, 0xa                     
    jz .is_line_feed                ;判断是否为 LF 换行

    cmp cl, 0x8                     ;BS backspace 的asc码是 8 
    jz .is_backspace    
    jmp .put_other 

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;



.is_backspace:
;;;;;;;;;;;;;;;  backspace 的一点说明 ;;;;;;;;;;;;;;;;; 
;当为ackspace 时,本质上只要将光标移向前一个显存位置既可,后面在输入的字符自然会覆盖此处的字符
;但有可能在键入backspace 后并不在键入新的字符,这是光标已经向前移动到待删除的字符位置,但字符还在原处,所以此处添加空格或者空字符0 

    dec bx 
    shl bx, 1                           ;光标左移1位等于乘2 ,表示光标对应显存中的偏移字节
    
    mov byte [gs:bx], 0x20              ;将待删除的字节补为0或空格  
    inc bx 
    mov byte [gs:bx], 0x07 
    shr bx, 1 
    jmp .set_cursor 

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;; 

.put_other:                             ;处理可见字符

    shl bx, 1                           ;光标位置用 2 字节表示,将光标值乘2 ,表示光标对应显存中的偏移字节 

    mov [gs:bx], cl                     ;Ascii 字符本身 
    inc bx  
    mov byte [gs:bx], 0x07              ;字符属性 
    shr bx, 1                           ;恢复老的光标 
    inc bx                              ;下一个光标值 
    cmp bx, 2000  
    jl .set_cursor                      ;若光标值小于 2000,表示未写到显存的最后,则去设置新的光标值
                                        ;若超出屏幕字符数大小(2000),则换行处理, 
    .is_line_feed:                      ;LF 
    .is_carriage_return:                ;CR 

    ;如果是CR,只要把光标移到行首就行 
    xor dx, dx                          ;dx 是被除数的高 16位,清0
    mov ax, bx                          ;ax 是被除数的低16位 
    mov si, 80                          ;\n 表示下一行行首 

    div si 
    sub bx, dx                          ;光标值减去 除80的余数便是取整 

.is_carriage_return_end:                ;回车符处理结束

    add bx, 80 
    cmp bx, 2000 

.is_line_feed_end:                      ;若是LF,将光标移 + 80 既可 
    jl .set_cursor 


;屏幕行范围是0 ~ 24,滚屏的原理是将屏幕的第1 ~ 24 行搬运到第0 ~ 23行 
;再将第24行用空格填充 

.roll_screen:                           ;若超出屏幕大小，开始滚屏 

    cld                                 ;清除方向位 ,把eflags　中的DF位清0 
    mov ecx, 960                        ;2000 - 80 = 1920 个字符要搬运,共1920*2 = 3840字节,一次搬运4字节,共3840/4 = 960 次
    mov esi, 0xc00b80a0                 ;第1行行首,将要复制的起始地址赋给 esi 寄存器
    mov edi, 0xc00b8000                 ;第0行行首,目的地址 
    rep movsd                           ;rep 开始循环复制

;;;;;;;;;;;;;;将最后一行填充为空白 

    mov ebx, 3840                       ;最后一行首字符第一个字节的偏移
    mov ecx, 80                         ;一行是80字符(160字节), 每次清空1字符(2字节),一行需要移动80次 

.cls:                                   ;清空最后一行

    mov word [gs:ebx], 0x0720           ;0x0720是黑底白字的空格键
    add ebx, 2 
    loop .cls  
    mov bx, 1920                        ;将光标重置为1920,最后一行的首字符 


.set_cursor:                            ;设置光标的位置
;;;;;;;;;;;;;;;;1 先设置高 8 位 ;;;;;;;;;;;;;;; 

    mov dx, 0x03d4                      ;索引寄存器
    mov al, 0x0e                        ;用于提供光标的高8位 
    out dx, al                          

    mov dx, 0x03d5                      ;通过读写数据端口0x3d5 来获取或设置光标位置 
    mov al, bh 
    out dx, al  



;;;;;;;;;;;;;;;;2 在设置低 8位 ;;;;;;;;;;;;;;;;;;;;;; 
    
    mov dx, 0x03d4 
    mov al, 0x0f 
    out dx, al 
    
    mov dx, 0x03d5 
    mov al, bl
    out dx, al 

.put_char_done:
    popad 
    ret 
    
