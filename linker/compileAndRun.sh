make
./linker -hex -place=ivt@0x0000 -o program.hex ../Assembler/bin_interrupts.o ../Assembler/bin_main.o