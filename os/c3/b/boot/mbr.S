;主引导程序 

%include "boot.inc"

;初始化寄存器
SECTION MBR vstart=0x7c00
    mov ax, cs
    mov ds, ax 
    mov es, ax
    mov ss, ax 
    mov fs, ax 
    mov sp, 0x7c00  ;mbr 引导地址
    mov ax, 0xb800  ;文本模式显示的显存地址
    mov gs, ax 

;清屏
;利用0x06号功能， 上卷全部行，则可清屏
;INT 0x10 功能号 0x06  功能描述:上卷窗口
;输入
;AH 功能号=0x06 
;AL = 上卷的行数，如果为，表示全部
;BH = 上卷行属性
;(CL, CH) = 窗口左上角的(x,y)位置
;(DL,DH) = 窗口右下角的(x,y)位置 
;无返回值: 
    mov ax, 0600h 
    mov bx, 0700h 
    mov cx, 0           ;左上角 
    mov dx, 184fh       ;右下角
;VGA 文本模式中，一行只能容纳80个字符，共25行
;下标从0开始，所以0x18=24， 0x4f=79 

    int 10h                 ;int 10h 

;输出背景色绿色,前景色红色,并且跳动的字符串"1 MBR "

    ;往显存中写字符    
    mov byte [gs:0x00], '1'
    mov byte [gs:0x01], 0xA4   ;A表示绿色背景闪烁,4表示前景色为红色
    
    mov byte [gs:0x02], ' '    
    mov byte [gs:0x03], 0xA4
    
    mov byte [gs:0x04], 'M'
    mov byte [gs:0x05], 0xA4 

    mov byte [gs:0x06], 'B'
    mov byte [gs:0x07], 0xA4 

    mov byte [gs:0x08], 'R'
    mov byte [gs:0x09], 0xA4 

    
    ; eax, bx, cx 为函数 rd_dsk_m_16 传递参数
    mov eax, LOADER_START_SECTOR   ;起始扇区lba 地址 0x2 , 即第二块扇区
    mov bx,  LOADER_BASE_ADDR      ;写入的地址  --即从硬盘中读取的数据存放在内存什么地方 0x900,
    mov cx,  1                     ;待读入的扇区数
    call rd_disk_m_16              ;以下读取程序的起始部分(一个扇区) 

    jmp LOADER_BASE_ADDR    

;---------------------------------------
;功能:读取硬盘 n 个扇区

rd_disk_m_16:
            ;eax = LBA扇区号
            ;bx  = 将数据写入的内存地址
            ;cx  = 读入的扇区数 

    mov esi, eax            ;备份eax
    mov di, cx              ;备份cx 


;读写硬盘: 
;第一步设置要读取的扇区数
    mov dx, 0x1f2 
    mov al, cl
    out dx, al      ;读取的扇区数

    mov eax, esi    ;恢复ax

;第2步:将LBA地址存入0x1f3 ~ 0x1f6 
        ;LBA 地址 7 ~ 0 位写入端口 0x1f3 

        mov dx, 0x1f3 
        out dx, al 


        ;LBA地址15 - 8位写入端口0x1f4 
        mov cl,  8
        shr eax, cl 
        mov dx,  0x1f4 
        out dx,  al 
        ;LBA地址23 -16 位写入端口 0x1f5
        shr eax, cl 
        mov dx, 0x1f5 
        out dx, al 


        shr eax, cl 
        and al, 0x0f        ;lba 第24 ~ 27 位
        or al, 0xe0         ;设置7 - 4 位为1110,表示lba模式

        mov dx, 0x1f6 
        out dx, al 
    
;第三步:向0x1f7 端口写入读命令 0x20 
        mov dx, 0x1f7     ;command 端口为 0x1f7 
        mov al, 0x20      ;读扇区的命令
        out dx, al 

;第4步:检测硬盘状态
    .not_ready:
        ;同一端口,写时表示写入命令字,读时表示读入硬盘状态
        nop                     ;nop 空操作,什么也不做,只是为了增加延迟, 相当于sleep 了一下,目的是减少打扰硬盘工作
        in al, dx               ;将 Status 寄存器的值读入到 al 寄存器 
        and al, 0x88            ;第4位为1表示硬盘控制器已准备好数据传输, and 操作,保留第4位和第7位.
                                ;第7位为1 表示硬盘忙碌
        cmp al, 0x08            ;判断第 4 为是否为1  cmp 指令和 0x08做减法,
                                ;cmp 不改变操作数的值,只是根据结果去设置标志位.
        jnz .not_ready          ;若还没有准备好,继续等待
        ;jnz 判断上条指令是否为0, 若不等于 0 ,则跳回去继续判断,若为0,则跳出循环继续执行. 
        ;若结果不为 0 时,代表磁盘正在忙 .

;第5步:从0x1f0 端口读数据, 从硬盘读取数据的过程
        mov ax, di          ;di 读取的扇区数 
        mov dx, 256 
        mul dx 
        mov cx, ax 

;di 为要读的扇区数, 一个扇区有512 字节,每次读入一个字, 需要 di*512/2 次,所以di*256
        
        mov dx, 0x1f0 
    .go_on_read:
        in ax, dx               ;把dx端口的数据写 ax中
        mov [bx], ax            ;把 ax 中的数据写到 [bx] 这里. 以bx的内容为地址的这块内存. 
        add bx, 2               ; bx + 2
        loop .go_on_read 
       
        ret 


        times 510 - ($ - $$) db 0

        db 0x55, 0xaa 
