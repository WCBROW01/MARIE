# MARIE

An emulator and assembler for the MARIE toy ISA (Instruction Set Architecture), found in the book *Essentials of Computer Organization and Architecture* by Linda Null and Julia Lobur. (ISBN-13: 978-1284123036, ISBN-10: 1284123030)

As it currently stands, the emulator is mostly complete, but the assembler is not. However, they are both in a usable enough state to run any of the example programs you might see in the book.

## Building

### Using the makefile

running `make` or `make all` will build both the emulator and assembler.
`make emu` will build the emulator.
`make asm` will build the assembler.

### Without a makefile

Building the emulator just requires the one source file, so you can just run `cc emu.c -o emu`.

The assembler currently requires my [cvector](https://github.com/WCBROW01/cvector) library, but the current approach I'm taking is needlessly complex, so this dependency will likely disappear. This library consists of a single source and header file, so it only requires one extra file.

It can be built by running `cc asm.c cvector.c -o asm`.
