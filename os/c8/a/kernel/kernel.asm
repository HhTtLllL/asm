[bits 32] 

%define ERROR_CODE nop      ;若在相关的异常中CPU已经自动压入了错误码,为保持栈中格式统一,这里不做操作
%define ZERO push 0         ;若在相关的异常中CPU中没有压入错误码,为了栈中格式统一,手工压入一个0 

extern idt_table              ;idt_table 是c中注册的中断处理程序数组,在interrupt.c 中定义的 idt_table,需要声明

section .data
;intr_str db "interrupt occur!", 0xa, 0 

global intr_entry_table 
intr_entry_table:

;这是一个宏
%macro VECTOR 2             ;宏定义, 名字 + 参数个数 ,第一个参数为中断向量号,第二个参数占位充数

section .text 

;在汇编中调用c 程序,回破坏现有寄存器,所以需要保护
intr%1entry:                ;intr[0-32]entry 每个终端程序都要压入中断向量号,所以一个中断类型一个中断处理程序,自己知道自己的中断向量号是多少

    %2                      ;中断若有错误码,会压在eip后面
;以下是保存上下文环境
    push ds
    push es
    push fs
    push gs 
    pushad                  ;pushad 指令压入32位寄存器,其入栈顺序是:EAX, ECX, EDX, EBX, ESP, EBP, ESI, EDI

;如果是 从片上进入的中断,除了往从片上发送EOI外,还要往主片上发送EOI 
    mov al, 0x20            ;中断结束命令EOI 
    out 0xa0, al            ;向从片发送, ocw2 写入从片的0x20 端口,内容为 0x20 ,即写入 EOI 
    out 0x20, al            ;向主片发送, ocw2 写入主片的0xA0 端口,内容为 0x20 

    push %1                 ;不管idt_table中的目标程序是否需要参数,都一律压入中断向量号,调试时很方便
    call [idt_table + %1*4] ;调用idt_table 中的C版本中断处理函数
    jmp intr_exit 

section .data 

    dd intr%1entry          ;存储各个中断入口程序的地址,形成intr_entry_table 数组

%endmacro

section .text 
global intr_exit 
intr_exit:
;以下是恢复上下文环境 
    add esp, 4              ;跳过中断号 
    popad 
    pop gs 
    pop fs 
    pop es 
    pop ds 
 ;   popad

    add esp, 4              ;跳过error_code 

    iretd 

;正文

VECTOR 0x00, ZERO
VECTOR 0x01, ZERO
VECTOR 0x02, ZERO
VECTOR 0x03, ZERO
VECTOR 0x04, ZERO
VECTOR 0x05, ZERO
VECTOR 0x06, ZERO
VECTOR 0x07, ZERO
VECTOR 0x08, ERROR_CODE 
VECTOR 0x09, ZERO
VECTOR 0x0a, ERROR_CODE 
VECTOR 0x0b, ERROR_CODE 
VECTOR 0x0c, ZERO 
VECTOR 0x0d, ERROR_CODE 
VECTOR 0x0e, ERROR_CODE 
VECTOR 0x0f, ZERO
VECTOR 0x10, ZERO
VECTOR 0x11, ERROR_CODE 
VECTOR 0x12, ZERO
VECTOR 0x13, ZERO
VECTOR 0x14, ZERO
VECTOR 0x15, ZERO
VECTOR 0x16, ZERO
VECTOR 0x17, ZERO
VECTOR 0x18, ERROR_CODE 
VECTOR 0x19, ZERO
VECTOR 0x1a, ERROR_CODE 
VECTOR 0x1b, ERROR_CODE 
VECTOR 0x1c, ZERO
VECTOR 0x1d, ERROR_CODE 
VECTOR 0x1e, ERROR_CODE 
VECTOR 0x1f, ZERO
VECTOR 0x20, ZERO

