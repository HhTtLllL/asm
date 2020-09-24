%include "boot.inc"

section loader vstart=LOADER_BASE_ADDR
;LOADER_STACK_TOP 是用于loader在保护模式下的栈,等于 LOADER_BASE_ADDR,其实这是loader在实模式下时的栈指针地址.只不过在进入保护模式后,
;咱们得为保护模式下的esp初始化,所以用了相同的内存地址作为栈顶, LOADER_BASE_ADDR的值是0x900,这是loader被加载到内存中的位置,在此地址之下
;便是栈
LOADER_STACK_TOP equ LOADER_BASE_ADDR 


;构建 gdt 及其内部的描述符,并且直接在里面填充段描述符
;第0个段描述符 不可用 ---置为0
GDT_BASE: 
    dd 0x00000000
    dd 0x00000000

;第一个段描述符 ---代码段
CODE_DESC:
    dd 0x0000FFFF         ;段描述符的低4字节        FFFF是段界限的0-15位  0000是
    dd DESC_CODE_HIGH4

;第二个段描述符    ---数据段和栈段
DATA_STACK_DESC:
    dd 0x0000FFFF
    dd DESC_DATA_HIGH4

;第三个段描述符   ---显存段描述符
VIDEO_DESC:
    dd 0x80000007       ;limit=(0xbffff - 0xb80000)/4k = 0x7 
    dd DESC_VIDEO_HIGH4 ;此时DPL为0 

GDT_SIZE equ $ - GDT_BASE 
GDT_LIMIT equ GDT_SIZE - 1                 ;第0个段描述符 不可用


times 60 dq 0                              ;此处预留60个描述符的空位

SELECTOR_CODE  equ (0x0001 << 3) + TI_GDT + RPL0    ;相当于(CODE_DESC - GDT_BASE)/8 + TI_GDT + RPL0 
SELECTOR_DATA  equ (0x0002 << 3) + TI_GDT + RPL0 
SELECTOR_VIDEO equ (0x0003 << 3) + TI_GDT + RPL0 

;total_mem_bytes 用于保存内存容量,以字节为单位,位置比较好记
;当前偏移loader.bin 文件头 0x200 字节, loader.bin 的加载地址是0x900 
;故,total_mem_bytes 内存中的地址是 0xb00,将来在内核中会引用此地址
;地址为什么是0xb00,   因为4个段描述符, 和60个段描述符空位, (4 + 60)*8  = 512 字节 = 0x200 ,loader 加载的地方是0x900 
;所以 total total_mem_bytes 的地址为 0xb00 
total_mem_bytes dd 0


;以下是GDT的指针,前两个字节是gdt界限,后四字节是GDT的起始位置
;这个指针是ldgt加载GDT到gdtr寄存器时用的 
;提前定义的缓冲区,目的是存储0xe820子功能返回的ards结构
gdt_ptr dw GDT_LIMIT 
        dd GDT_BASE 

;人工对齐:total_mem_bytes(4) + gdt_ptr(6) + ards_buf(244) + ards_nr(2) = 256 字节
ards_buf times 244 db 0
ards_nr dw 0                ;用于记录ARDS结构提数量

loader_start:

;--------------- int 15h  eax = 0000E820h,  edx = 534D4150h  ('SMAP'获取内存布局) ----------------

    xor ebx, ebx             ;第一次调用时,ebx 值为0 
    mov edx, 0x534d4150      ;edx 只赋值一次,循环体中不会改变
    mov di, ards_buf         ;ards 结构缓冲区,es 在mbr 中已经赋值了
.e820_mem_get_loop:          ;循环获取每个ARDS内存范围描述结构
    
    mov eax, 0x0000e820      ;执行int 0x15后,eax值变为 0x534d4150 
;所以每次执行 int 前都要更新为子功能号 
    mov ecx, 20              ;ARDS地址范围描述符结构大小是 20 个字节,ecx 为实际写入缓冲区中的字节数
    int 0x15 
    jc .e820_failed_so_try_e801 
;若cf位 为1,则有错误发生,尝试0xe801 子功能
    add di, cx               ;使di增加20字节指向缓冲区中新的ARDS结构位置 
    inc word [ards_nr]       ;记录ARDS数量
    cmp ebx, 0               ;若eax为0,且cf不为1,这说明ards全部返回 ,当前已经是最后一个 
    jnz .e820_mem_get_loop 

;在所有ards 结构中, 找出(base_add_low + length_low) 的最大值,即内存的容量 
    mov cx, [ards_nr] 

;遍历每一个ards 结构提,循环次数是 ards 的数量 
    mov ebx, ards_buf 
    xor edx, edx             ;edx 为最大的内存容量,在此先清0  

.find_max_mem_area:         ;无需判断type是否为1,最大的内存快一定是可被使用的
    
    mov eax, [ebx]           ;base_add_low 
    add eax, [ebx + 8]       ;length_low  
    add ebx, 20              ;指向缓冲区的下一个ards结构
    cmp edx, eax            
;冒泡排序,找出最大,edx 寄存器始终是最大内存的容量
    jge .next_ards 
    mov edx, eax            ;edx 为总内存大小 
.next_ards:
    loop .find_max_mem_area 
    jmp  .mem_get_ok 


;------ int 15h ax = E801h 获取内存大小,最大支持4G ------------- 
;返回后, ax,cx 值一样,以KB为单位, bx,dx 值一样, 以64KB为单位 
;在ax 和cx 寄存器中为低 16MB, 在bx 和 dx寄存器中为 16MB到4GB 
.e820_failed_so_try_e801:
    
    mov ax, 0xe801 
    int 0x15  
    jc .e801_failed_so_try88                ;若当前e801方法失败,就尝试0x88方法 

;1 先算出低15MB的内存, ax和cx中是以KB为单位的内存数量,将其转换为以byte为单位 
    mov cx, 0x400                           ;cx 和ax值一样,cx用作乘数 
    mul cx 
    shl edx, 16 
    and eax, 0x0000FFFF 
    or edx, eax 
    add edx, 0x100000                       ;ax 只是15MB,故要加1MB 
    mov esi, edx                            ;先把低15M的内存容量存入esi寄存器备份


;2 再将16MB以上的内存转换为 byte 为单位 
;寄存器bx 和 dx 中是以64KB为单位的内存数量
    
    xor eax, eax 
    mov ax, bx 
    mov ecx, 0x10000                        ;0x10000 十进制为64KB
    mul ecx                                 ;32位乘法,默认的被乘数是eax,积为64位 
                                            ;高32位存入edx,低32位存入eax   


    add esi, eax    

;由于此方法只能测出4GB以内的内存,故32位eax足够了
;edx 肯定为0, 只加 eax便可  
    mov edx, esi                            ;edx 为总内存大小
    jmp .mem_get_ok   

;-------- int 15h ah = 0x8 获取内存大小,只获取64MB之内-------------
.e801_failed_so_try88:
;int 15后,ax 存入的是以KB为单位的内存容量 
    mov ah, 0x88 
    int 0x15 
    jc .error_hlt 
    and eax, 0x0000FFFF 


;16位乘法,被乘数是ax, 积为32位.积的高16位在dx中, 积的低16位在ax中
    mov cx, 0x400 

;0x400 等于1024,将ax中的内存容量换为以byte为单位  
    mul cx
    shl edx, 16             ;把dx移到高16位 
    or edx, eax             ;把积的低16位组合到edx,为32位的积
    add edx, 0x100000       ;0x88子功能只会返回1MB以上的内,故实际内存大小要加上1MB

.mem_get_ok:
    mov [total_mem_bytes], edx   ;将内存换为byte单位后存入total_memytes处 
    
;-------------------- 准备进入保护模式---------------------------
;1 打开A20 
;2 加载 gdt 
;3 将cr0 的pe位置为1 
;------------- 打开A20 -------------
    
    in al, 0x92 
    or al, 0000_0010b
    out 0x92, al 
;---------加载gdt ------------------
    
    lgdt [gdt_ptr] 

;---------cr0 第0位置为1 -----------
    mov eax, cr0 
    or  eax, 0x00000001
    mov cr0, eax 


    jmp dword SELECTOR_CODE:p_mode_start      ;刷新流水线

.error_hlt:
    hlt
[bits 32]
p_mode_start:
    mov ax, SELECTOR_DATA 
    mov ds, ax 
    mov es, ax
    mov ss, ax 
    mov esp, LOADER_STACK_TOP 
    mov ax, SELECTOR_VIDEO 
    mov gs, ax



;--------------------------- 加载内核 -----------------------------

    mov eax, KERNEL_START_SECTOR            ;kerner.bin 所在的扇区号
    mov ebx, KERNEL_BIN_BASE_ADDR           ;从磁盘读出后,写入到ebx指定的地址
    mov ecx, 200                            ;读入的扇区数


    call rd_disk_m_32


    call setup_page                         ;创建页目录及页表并初始化页内存位图

;要将描述符表地址及偏移量写入内存gdt_ptr,一会用新地址重新加载 
    sgdt [gdt_ptr]                          ;存储到原来gdt所有的位置 
;将gdt描述符中视频段描述符中的段基址 + 0xc0000000 
    mov ebx, [gdt_ptr + 2] 
    or dword [ebx + 0x18 + 4], 0xc0000000 

;视频段是第三个段描述符,每个描述符是8字节,故0x18 
;段描述符的高4字节的最高位是段基址的第31 ~ 24 位 

;将gdt 的基址加上 0xc0000000 使其成为内核所在的高地址
    add dword [gdt_ptr + 2], 0xc0000000 

    add esp, 0xc0000000             ;将栈指针同样映射到内核地址 

;把页目录地址赋给 cr3 
    mov eax, PAGE_DIR_TABLE_POS 
    mov cr3, eax 

;打开cr0 的pg 位(第31位) 

    mov eax, cr0 
    or eax, 0x80000000 
    mov cr0, eax 

;在开启分页后,用gdt新的地址重新加载 
    lgdt [gdt_ptr]                ;重新加载 

;---------------- 此时不刷新流水线也没问题 ---------------------- 
;由于一直处在32位下,原则上不需要强制刷新,经过实际测试没有以下这两句也没问题.
;但以防万一，还是加上啦，免得将来出来莫句奇妙的问题.

    jmp SELECTOR_CODE:enter_kernel              ;强制刷新六熟悉呢,更新gdt

enter_kernel:
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
    call kernel_init
    mov esp, 0xc009f000 
    jmp KERNEL_ENTRY_POINT                      ;用地址0x1500访问测试



;----------------- 将kernel.bin 中的segment 拷贝到编译的地址

;-----------------创建页目录及页表 -------------
setup_page:
;先把页目录占用的空间逐字节清 0 

    mov ecx, 4096
    mov esi, 0
.clear_pagr_dir:
    mov byte [PAGE_DIR_TABLE_POS + esi], 0
    inc esi 
    loop .clear_pagr_dir          ;计数在ecx中

;开始创建页目录项(PDE) 
.create_pde:                      ;创建Page Directory Entry
    mov eax, PAGE_DIR_TABLE_POS  
    add eax, 0x1000               ;此时eax为第一个页表的位置及属性
    mov ebx, eax                  ;此处为ebx赋值,是为.create_pte做准备.ebx为基址


;下面将页目录项0和页目录项0xc00 都存为第一个页表的地址
;一个页表可表示4MB内存,这样0xc03fffff以下的地址和0x003fffff 以下的地址
;都指向相同的页表
;这是为将地址映射为内核地址做准备
    or eax, PG_US_U | PG_RW_W | PG_P  ;页目录项的属性RW和P位为1,US为1,表示用户属性,所有特权级别都可以访问
    mov [PAGE_DIR_TABLE_POS + 0x0], eax         ;第1个目录项 
    ;在页目录表中的第一个目录项写入第一个页表的位置(0x101000)及属性(7)
    mov [PAGE_DIR_TABLE_POS + 0xc00], eax  
;一个页表占用4字节
;0xc00表示第768个页表占用的目录项,0xc00 以上的目录项用于内核空间
;也就是页表的0xc0000000 ~ 0xffffffff 共计 1G 属于内核
;0x0 ~ 0xbfffffff 共计3G属于用户进程 
    sub eax, 0x1000 
    mov [PAGE_DIR_TABLE_POS + 4092], eax 
;使最后一个目录项指向页目录表自己的地址


;下面创建页表项(PTE) 
    mov ecx, 256                        ;1M底端内存 / 每页大小4k = 256 
    mov esi, 0 
    mov edx, PG_US_U | PG_RW_W | PG_P   ;属性为7, US=1,RW=1,P=1  
.create_pte:                            ;创建Page Table Entry 
    
    mov [ebx + esi*4], edx  
;此时的ebx已经在上面通过eax赋值为0x101000,也就是第一个页表的地址
    add edx, 4096 
    inc esi 
    loop .create_pte 


;创建内核其他页表的PDE 
    
    mov eax, PAGE_DIR_TABLE_POS 
    add eax, 0x2000                     ;此时eax为第二个页表的位置
    or eax, PG_US_U | PG_RW_W | PG_P    ;页目录项的属性US,RW,和P为都为1 

    mov ebx, PAGE_DIR_TABLE_POS   
    mov ecx, 254                        ;范围为第769 - 1022 的所有目录项数量
    mov esi, 769 

.create_kernel_pde:

    mov [ebx + esi*4], eax 
    inc esi 
    add eax, 0x1000 
    loop .create_kernel_pde 
    ret 

;--------------------------------------------------------------------------------
                            ;功能:读取硬盘n个扇区
rd_disk_m_32:   
;---------------------------------------------------------------------------
;eax = LBA扇区号 
;ebx = 将数据写入的内存地址
;ecx = 读入的扇区数 

    mov esi, eax                    ;备份eax 
    mov di, cx                      ;备份扇区数到 di

;读写硬盘:
;第一步:设置要读取的扇区数
    
    mov dx, 0x1f2 
    mov al, cl 
    out dx, al                      ;读取的扇区数

    mov eax, esi                    ;恢复ax 

;第二步:将LBA地址存入0x1f3 ~ 0x1f6 
    ;LBA地址 7 - 0      位写入端口0x1f3 
    mov dx, 0x1f3 
    out dx, al 

    ;LBA地址 15 - 8      位写入端口0x1f4 
    mov cl, 8
    shr eax, cl 
    mov dx, 0x1f4 
    out dx, al 


    ;LBA地址23 ~ 16 写入端口0x1f5 
    shr eax, cl 
    mov dx, 0x1f5 
    out dx, al 

    shr eax, cl 
    and al, 0x0f                    ;lba第24 ~ 27 位
    or al, 0xe0                     ;设置 7 ~ 24位为 1110,表示LBA模式
    mov dx, 0x1f6                   
    out dx, al 

;第三步:向0x1f7 端口写入读命令, 0x20 
    mov dx, 0x1f7 
    mov al, 0x20 
    out dx, al 




;至此,硬盘控制器便从指定的lba地址(eax)处,读出连续的cx个扇区,下面检查硬盘状态,当硬盘不忙碌的时候,就能把这个cx个扇区的数据读出

;第四步: 检测硬盘状态
.not_ready:                         ;检测0x1f7 端口(status 寄存器)的BSY位 
                                    ;同一端口,写时表示写入命令字,读时表示读入硬盘的状态 
    nop
    in al, dx 
    and al, 0x88                    ;第4位为1,表示硬盘控制器已经准备好数据传输,第7位为1表示硬盘正在忙碌 
    cmp al, 0x08 

    jnz .not_ready                  ;如果没有准备好,继续等待


;第5步,从 0x1f0端口读取数据
    mov ax, di                      ;以下从硬盘端口读数据用insw指令更快,不过尽可能多的演示命令使用
                                    ;这里先用这种方法,后面会用insw, outsw 
    mov dx, 256         

    mul dx
    mov cx, ax 
    mov dx, 0x1f0 

.go_on_read: 
    
    in ax, dx 
    mov [ebx], ax 
    add ebx, 2              
    
			  ; 由于在实模式下偏移地址为16位,所以用bx只会访问到0~FFFFh的偏移。
			  ; loader的栈指针为0x900,bx为指向的数据输出缓冲区,且为16位，
			  ; 超过0xffff后,bx部分会从0开始,所以当要读取的扇区数过大,待写入的地址超过bx的范围时，
			  ; 从硬盘上读出的数据会把0x0000~0xffff的覆盖，
			  ; 造成栈被破坏,所以ret返回时,返回地址被破坏了,已经不是之前正确的地址,
			  ; 故程序出会错,不知道会跑到哪里去。
			  ; 所以改为ebx代替bx指向缓冲区,这样生成的机器码前面会有0x66和0x67来反转。
			  ; 0X66用于反转默认的操作数大小! 0X67用于反转默认的寻址方式.
			  ; cpu处于16位模式时,会理所当然的认为操作数和寻址都是16位,处于32位模式时,
			  ; 也会认为要执行的指令是32位.
			  ; 当我们在其中任意模式下用了另外模式的寻址方式或操作数大小(姑且认为16位模式用16位字节操作数，
			  ; 32位模式下用32字节的操作数)时,编译器会在指令前帮我们加上0x66或0x67，
			  ; 临时改变当前cpu模式到另外的模式下.
			  ; 假设当前运行在16位模式,遇到0X66时,操作数大小变为32位.
			  ; 假设当前运行在32位模式,遇到0X66时,操作数大小变为16位.
			  ; 假设当前运行在16位模式,遇到0X67时,寻址方式变为32位寻址
			  ; 假设当前运行在32位模式,遇到0X67时,寻址方式变为16位寻址.

    loop .go_on_read 

    ret 




































