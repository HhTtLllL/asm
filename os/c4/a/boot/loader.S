%include "boot.inc"

section loader vstart=LOADER_BASE_ADDR
;LOADER_STACK_TOP 是用于loader在保护模式下的栈,等于 LOADER_BASE_ADDR,其实这是loader在实模式下时的栈指针地址.只不过在进入保护模式后,
;咱们得为保护模式下的esp初始化,所以用了相同的内存地址作为栈顶, LOADER_BASE_ADDR的值是0x900,这是loader被加载到内存中的位置,在此地址之下
;便是栈
LOADER_STACK_TOP equ LOADER_BASE_ADDR 

mov byte [gs:0x00], 'K'

jmp loader_start 

mov byte [gs:0x00], 'D'

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

times 20 dq 0                              ;此处预留60个描述符的空位

SELECTOR_CODE  equ (0x0001 << 3) + TI_GDT + RPL0    ;相当于(CODE_DESC - GDT_BASE)/8 + TI_GDT + RPL0 
SELECTOR_DATA  equ (0x0002 << 3) + TI_GDT + RPL0 
SELECTOR_VIDEO equ (0x0003 << 3) + TI_GDT + RPL0 


;以下是GDT的指针,前两个字节是gdt界限,后四字节是GDT的起始位置
;这个指针是ldgt加载GDT到gdtr寄存器时用的 
gdt_ptr dw GDT_LIMIT 
        dd GDT_BASE 

loadermsg db '2 loader in real.'


loader_start:
    mov byte [gs:0x00], 'Z'

;------------------------------------------------
;int 0x10 功能号:0x13  功能描述:打印字符串 
;------------------------------------------------
;输入:
;AH 子功能号=13H
;BH = 页码
;BL = 属性(若 AL=00H 或 01H)
;CX=字符串长度
;(DH､ DL)=坐标(行 列 ､ )
;ES:BP=字符串地址
;AL=显示输出方式
; 0—字符串中只含显示字符，其显示属性在 BL 中
;显示后，光标位置不变
; 1—字符串中只含显示字符，其显示属性在 BL 中
;显示后，光标位置改变
; 2—字符串中含显示字符和显示属性。显示后，光标位置不变
; 3—字符串中含显示字符和显示属性。显示后，光标位置改变
;无返回值
    mov byte [gs:0x00], 'R'
    mov sp, LOADER_BASE_ADDR 
    mov bp, loadermsg             ;es:bp = 字符串地址
    mov cx, 17
    mov ax, 0x1301                ;AH = 13, AL = 01
    mov bx, 0x001f                ;页号为0, 蓝底粉红字(BL = 1fh)
    mov dx, 0x1800                ;行数dh为0x18,列数dl为0x00.这也是int 0x10的参数.由于在文本模式下的行数是25行,
    int 0x10                                 ;即0~24,所以0x18的十进制为24,最后一行

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






