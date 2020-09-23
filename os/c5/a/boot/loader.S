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
;当前便宜loader.bin 文件头 0x200 字节, loader.bin 的加载地址是0x900 
;故,total_mem_bytes 内存中的地址是 0xb00,将来在内核中会引用此地址
total_mem_bytes dd 0


;以下是GDT的指针,前两个字节是gdt界限,后四字节是GDT的起始位置
;这个指针是ldgt加载GDT到gdtr寄存器时用的 
gdt_ptr dw GDT_LIMIT 
        dd GDT_BASE 

;人工对齐:total_mem_bytes(4) + gdt_ptr(6) + ards_buf(244) + ards_nr(2) = 256 字节
ards_buf times 244 db 0
ards_nr dw 0                ;用于记录ARDS结构提数量

loader_start:

;int 15h  eax = 0000E820h,  edx = 534D4150h  ('SMAP'获取内存布局) 

    xor ebx, ebx             ;第一次调用时,ebx 值为0 
    mov edx, 0x534d4150      ;edx 只赋值一次,循环体中不会改变
    mov di, ards_buf         ;ards 结构缓冲区
.e820_mem_get_loop:          ;循环获取每个ARDS内存范围描述结构
    
    mov eax, 0x0000e820      ;执行int 0x15后,eax值变为 0x534d4150 
;所以每次执行 int 前都要更新为子功能号 
    mov ecx, 20              ;ARDS地址范围描述符结构大小是 20 个字节
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

    mov byte [gs:160], 'P'         ;往显存的第80个字符的位置写入字符P,一个字符两个字节,所以此处偏移160 


    jmp $






