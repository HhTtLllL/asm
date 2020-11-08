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
VECTOR 0x20, ZERO                           ;时钟中断对应的入口
VECTOR 0x21, ZERO                           ;键盘中断对应的入口
VECTOR 0x22, ZERO                           ;级联用的
VECTOR 0x23, ZERO                           ;串口2对应的入口
VECTOR 0x24, ZERO                           ;串口1对应的入口
VECTOR 0x25, ZERO                           ;并口2对应的入口
VECTOR 0x26, ZERO                           ;软盘对应的入口
VECTOR 0x27, ZERO                           ;并口1对应的入口
VECTOR 0x28, ZERO                           ;实时时钟对应的入口
VECTOR 0x29, ZERO                           ;重定向
VECTOR 0x2a, ZERO                           ;保留
VECTOR 0x2b, ZERO                           ;保留
VECTOR 0x2c, ZERO                           ;ps/2鼠标
VECTOR 0x2d, ZERO                           ;fpu浮点单元异常
VECTOR 0x2e, ZERO                           ;硬盘
VECTOR 0x2f, ZERO                           ;保留


;;;;;;;;     0x80 中断 ;;;;;;;;;;;;;;;;;;;;;;
[bits 32]

extern syscall_table                        ;数组，数组成员是系统调用中子功能对应的处理函数，
                                            ;这里我们用子功能号在此数组中索引子功能号对应的处理函数
section .text 
global syscall_handler 

syscall_handler:                            ;中断例程
    ;1 保存上下文环境
    push 0                                  ;压入0,(错误码),为了保证栈中格式统一
                                            ;保存任务上下文
    push ds
    push es
    push fs
    push gs 
    pushad                                  ;pushad 指令压入32位寄存器，其入栈顺序是 eax,ecx,edx,ebx,esp,ebp,esi,edi 


    push 0x80                               ;此位置压入0x80 也是为了保持统一的栈格式, 中断号

    ;2 为系统调用子功能传入参数,c调用约定，最右边的参数先入栈
    push edx                                ;第三个参数
    push ecx                                ;第二个参数
    push ebx                                ;第一个参数

    ;3 调用子功能处理函数   
    call [syscall_table + eax*4]            ;syscall_table 存储的是函数的地址，大小为4字节

    add esp, 12                             ;跨过上面的3个参数
    ;4 将call 调用后的返回值存入当前内核栈中eax 的位置
    mov [esp + 8*4], eax    
    jmp intr_exit                           ;intr_exit 返回，恢复上下文

