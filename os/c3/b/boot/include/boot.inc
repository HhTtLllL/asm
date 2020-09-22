LOADER_BASE_ADDR equ 0x900          ;定义了 loader 在内存中的位置 说明loader 加载的时候会在内存地址 0x900处 
LOADER_START_SECTOR equ 0x2         ;定义了 定义了loader在硬盘上的逻辑扇区地址,即 LBA地址.等于 0x2 说明放在第2块扇区
