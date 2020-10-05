TI_GDT          equ 0 
RPL0            equ 0
SELECTOR_VIDEO  equ (0x0003 << 3) + TI_GDT + RPL0 

section .data 

    put_int_buffer dq 0         ;定义8字节缓冲区用于数字到字符的转换

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
 ;------------------------ 将小端字节序的数字变成对应的ascii后,倒置 ------------------------
 ;输入:栈中参数为待打印的数字 
 ;输出:在屏幕上打印16进制数组,并不会打印前缀0x,如打印10进制15时,只会打印f,不会是0xf 
 ;---------------------------------------------------------------------------------------------- 

 global put_int 
 put_int:

    pushad 
    mov ebp, esp 
    mov eax, [ebp + 4*9]                ;call 的返回地址占4个字节, + pushad 的8个4字节
    mov edx, eax            
    mov edi, 7                          ;指定在put_nit_buffer 中初始的偏移量
    mov ecx, 8                          ;32位数字中,16进制数字的位数是8个
    mov ebx, put_int_buffer 

;将32位数字按照16进制的形式从低位到高位逐个处理,共处理8个16进制数字 
.16based_4bits:                         ;每个4位二进制是16进制数字的1位,遍历每一位16进制数字 

    and edx, 0x0000000F                 ;解析16进制数字的每一位.and 与操作后,edx 只有低4位有效 
    cmp edx, 9                          ;数字0 ~ 9 和 a ~ f需要分别处理成对应的字符
    jg .is_A2F 
    
    add edx, '0'                        ;ascii 码是8位大小,add 求和操作后,edx低8位有效 
    jmp .store 

.is_A2F:
    sub edx, 10                         ;A-F减去10所得到的差,再加上字符A的ascii码,便是A~F对应的ascii码 
    add edx, 'A'

;将每一位数字转换成对应的字符后,按照类似"大端"的顺序存储到缓冲区put_int_buffer 
;高位字符放在低地址,第位字符要放在高地址,这样和大端字节序类似,只不过这里是字符序 
.store:
;此时dl中应该是数字对应的字符的ascii码 
    
    mov [ebx + edi], dl 
    dec edi 
    shr eax, 4 
    mov edx, eax 
    loop .16based_4bits 

;现在put_int_buffer中已经全是字符,在打印之前,把高位连续的字符去掉,比如把字符000123 变成 123 
.ready_toprint:

    inc edi                             ;此时edi退减为-1(0xfffffff),加1十七为 0 
.skip_prefix_0:
    cmp edi, 8                          ;若已经比较第9个字符,表示待打印的字符串为全0 
    je .full0 

;找出连续的0字符,edi 作为非0的最高位字符的偏移 
.go_on_skip:

    mov cl, [put_int_buffer + edi] 
    inc edi 
    cmp cl, '0'
    je .skip_prefix_0                   ;继续判断下一位字符是否为字符 0 (不是数字0)
    dec edi                             ;edi 在上面的inc 操作中指向了下一个字符,若当前字符不为'0',要恢复edi指向当前字符
    jmp .put_each_num                   

.full0:
    mov cl, '0'                         ;输入的数字为全0时,则只打印0 

.put_each_num:

    push ecx 
    call put_char 
    add esp, 4 
    inc edi                             ;使edi 指向下一个字符 
    mov cl, [put_int_buffer + edi]      ;获取下一个字符到cl 寄存器
    cmp edi, 9 
    jl .put_each_num 

    popad 
    ret 
