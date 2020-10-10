;-------------- loader , kernel ---------------------

LOADER_BASE_ADDR        equ 0x900          ;定义了 loader 在内存中的位置 说明loader 加载的时候会在内存地址 0x900处 
LOADER_START_SECTOR     equ 0x2         ;定义了 定义了loader在硬盘上的逻辑扇区地址,即 LBA地址.等于 0x2 说明放在第2块扇区
;LOADER_STACK_TOP        equ LOADER_BASE_ADDR 

KERNEL_BIN_BASE_ADDR    equ 0x70000 
;KERNEL_IMAGE_BASE_ADDR equ 0x1500 
KERNEL_START_SECTOR     equ 0x9 
KERNEL_ENTRY_POINT      equ 0xc0001500 


;--------------- 页表配置 --------------------

PAGE_DIR_TABLE_POS      equ 0x100000


;DESC_字段名_字段相关信息
;-------------------- gdt 描述符属性----------------

;gdt 描述符一共有 64位, 8个字节 G 位在第23位
DESC_G_4K equ 1_00000000000000000000000b    ;23个0  

;D/B 字段,用来指示有效地址(段内偏移地址)及操作数的大小.
;对于代码段,此位是D, 若D为0,表示指令中的有效地址和操作数是16位的,指令有效地址用IP存储
;                    若D为1,表示指令中的有效地址和操作数是32位的,指令有效地址有EIP存储

;对于栈段,此位是B,用来指定操作数大小,此操作数涉及到栈指针寄存器的选择及栈的地址上限.
;若B为0,使用的是sp寄存器,也就是栈的起始地址是16位寄存器的最大寻址范围0xFFFF 
;若B为1,使用ESP寄存器,也就是栈的起始地址是32位寄存器的最大寻址范围.

DESC_D_32 equ 1_0000000000000000000000b     ;22个0


;用来设置是否是 64位代码段 L 为1表示64位,0表示32位,此处表示为32为 
;64位代码标记,此处标记为 0 便可 
DESC_L    equ 0_000000000000000000000b      ;21个0 

;AVL 字段:可用的,操作系统可以随意用此位,对硬件没有专门的用途
DESC_AVL equ 0_00000000000000000000b        ;CPU不用此位,暂置为0 

;段界限的第二部分, 第一部分在段描述符的低4字节中,和第一部分组成 全1,
DESC_LIMIT_CODE2 equ 1111_0000000000000000b
DESC_LIMIT_DATA2 equ DESC_LIMIT_CODE2
DESC_LIMIT_VIDEO2 equ 0000_000000000000000b

;表示段存在 P
DESC_P equ 1_000000000000000b

;DPL表示特权级
DESC_DPL_0 equ 00_0000000000000b
DESC_DPL_1 equ 01_0000000000000b
DESC_DPL_2 equ 10_0000000000000b
DESC_DPL_3 equ 11_0000000000000b
;S_CODE 对于代码段 当前描述符 是否为系统段 0是系统段, 1为非系统段
DESC_S_CODE equ 1_000000000000b
DESC_S_DATA equ DESC_S_CODE
DESC_S_sys equ 0_000000000000b


DESC_TYPE_CODE equ 1000_00000000b
;x=1,c=0,r=0,a=0    代码段是可执行的,非一致性,不可读,已访问位 a 淸0

DESC_TYPE_DATA equ 0010_00000000b 
;x=0,e=0,w=1,a=0 数据段是不可执行的，向上扩展的，可写，已访问位 a 清 0

DESC_CODE_HIGH4 equ (0x00 << 24) + DESC_G_4K + DESC_D_32 + DESC_L + DESC_AVL + DESC_LIMIT_CODE2 + \
DESC_P+DESC_DPL_0 + DESC_S_CODE + DESC_TYPE_CODE + 0x00

DESC_DATA_HIGH4 equ (0x00 << 24) + DESC_G_4K + DESC_D_32 +\
DESC_L + DESC_AVL + DESC_LIMIT_DATA2 + DESC_P + DESC_DPL_0 + DESC_S_DATA + \
DESC_TYPE_DATA + 0x00

DESC_VIDEO_HIGH4 equ (0x00 << 24) + DESC_G_4K + DESC_D_32 +\
DESC_L + DESC_AVL + DESC_LIMIT_VIDEO2 + DESC_P + \
DESC_DPL_0 + DESC_S_DATA + DESC_TYPE_DATA + 0x0b

;-------------- 选择子属性 ---------------

RPL0 equ 00b
RPL1 equ 01b
RPL2 equ 10b
RPL3 equ 11b
TI_GDT equ 000b
TI_LDT equ 100b




;--------------页表相关属性  -----------------------
PG_P    equ 1b 
PG_RW_R equ 00b 
PG_RW_W equ 10b 
PG_US_S equ 000b 
PG_US_U equ 100b 


;------------------------- program type 定义 ------------------------
PT_NULL equ 0



