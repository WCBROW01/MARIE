# MARIE

An emulator and assembler for the MARIE toy ISA (Instruction Set Architecture), found in the book *Essentials of Computer Organization and Architecture* by Linda Null and Julia Lobur. (ISBN-13: 978-1284123036, ISBN-10: 1284123030)

## Building

### Using the makefile

running `make` or `make all` will build both the emulator and assembler.
`make emu` will build the emulator.
`make asm` will build the assembler.

### Without a makefile

Building the emulator just requires the one source file, so you can just run `cc emu.c -o emu`.
The assembler can be built by running `cc asm.c -o asm`.
