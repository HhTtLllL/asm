DATA segment
    num db 82h, 68h, 88h 
    sum db ?
DATA ends

CODE segment 
    assume cs:CODE, ds:DATA

START:  mov ax, DATA
        mov ds, ax
        mov bx, offset num
        mov al, [bx]
        inc bx
        add al, [bx]
        inc bx
        add al, [bx]
        mov sum, al
        mov ah, 4ch

        int 21h

CODE ENDS
    END START
