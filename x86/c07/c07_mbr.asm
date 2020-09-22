         ;代码清单7-1
         ;文件名：c07_mbr.asm
         ;文件说明：硬盘主引导扇区代码
         ;创建日期：2011-4-13 18:02
         
         jmp near start         ; 为了跳过没有指令的区域
	
 message db '1+2+3+...+100='
        
 start:
         mov ax,0x7c0           ;设置数据段的段基地址 
         mov ds,ax

         mov ax,0xb800          ;设置附加段基址到显示缓冲区
         mov es,ax

         ;以下显示字符串 
         mov si,message          
         mov di,0               ; 偏移地址
         mov cx,start-message   ; 计算循环的次数
     @g:
         mov al,[si]
         mov [es:di],al
         inc di                 
         mov byte [es:di],0x07  ;将黑底白字写进去
         inc di                 ;; di +1
         inc si                 ;;si + 1
         loop @g

         ;以下计算1到100的和 
         xor ax,ax              ;将ax寄存器清零
         mov cx,1
     @f:
         add ax,cx
         inc cx
         cmp cx,100
         jle @f

         ;以下计算累加和的每个数位 
         xor cx,cx              ;设置堆栈段的段基地址
         mov ss,cx
         mov sp,cx

         mov bx,10
         xor cx,cx
     @d:
         inc cx
         xor dx,dx
         div bx
         or dl,0x30
         push dx
         cmp ax,0
         jne @d

         ;以下显示各个数位 
     @a:
         pop dx                     ;从栈中弹出数据到dx中
         mov [es:di],dl
         inc di
         mov byte [es:di],0x07
         inc di
         loop @a
       
         jmp near $ 
       

times 510-($-$$) db 0
                 db 0x55,0xaa
