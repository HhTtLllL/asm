#!/bin/bash
##############################################################
# File Name: go.sh
# Author: tt
# Email: tttt@xiyoulinux.org
# Created Time : 2020-09-29 19:33:00
# Description:
##############################################################

gcc -m32 -I lib/kernel/ -c -o build/timer.o device/timer.c 
gcc -m32 -I lib/kernel/ -I lib/ -I kernel/ -c -fno-builtin -o build/main.o kernel/main.c  
gcc -m32 -I lib/kernel/ -I lib/ -I kernel/ -c -fno-builtin -o build/interrput.o kernel/interrupt.c 
gcc -m32 -I lib/kernel/ -I lib/ -I kernel/ -c -fno-builtin -o build/init.o kernel/init.c  

nasm -f elf -o build/print.o lib/kernel/print.S  
nasm -f elf -o build/kernel.o kernel/kernel.asm   

ld -m elf_i386 -Ttext 0xc0001500 -e main -o build/kernel.bin build/main.o  build/init.o  build/interrput.o build/print.o  build/kernel.o  \
    build/timer.o 
