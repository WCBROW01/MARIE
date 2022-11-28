CFLAGS = -std=c99 -O2

all: emu asm

emu: emu.o

asm: asm.o

.PHONY: clean
clean:
	rm -f emu asm emu.o asm.o
