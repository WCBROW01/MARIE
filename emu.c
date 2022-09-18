#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include "instructions.h"

enum emu_flags {
	FLAG_REGS = 1,
	FLAG_STEP = 2
};

static uint16_t mem[4096]; // 4K words of memory
static struct CPU {
	int16_t AC;
	uint16_t IR;
	int16_t MBR;
	uint16_t PC; // only 12 bits
	uint16_t MAR; // only 12 bits
	uint8_t InREG;
	uint8_t OutREG;
} cpu;

const char *HELP = "Usage: %s [OPTIONS] [FILE]\n"
	"\t-h, --help\tDisplays help page.\n"
	"\t-r, --regs\tDisplay registers.\n"
	"\t-s, --step\tRun the machine step by step.\n";

static char *disassemble_instruction(uint16_t ins) {
	static char buf[16];
	
	int opcode = ins >> 12;
	if (
		opcode == INPUT || opcode == OUTPUT || opcode == HALT || opcode == CLEAR
	) strcpy(buf, INSTRUCTIONS[opcode]);
	else sprintf(buf, "%s %03x", INSTRUCTIONS[opcode], ins & 0x0FFF);
	
	return buf;
}

static void print_cpu_state(void) {
	printf(
		"\nInstruction: %s\nAC: %d\nIR: %04x\nMBR: %d\nPC: %03x\nMAR: %03x\n"
		"InREG: %02x\nOutREG: %02x\n",
		disassemble_instruction(cpu.IR), cpu.AC, cpu.IR, cpu.MBR, cpu.PC,
		cpu.MAR, cpu.InREG, cpu.OutREG
	);
}

static void execute(unsigned int flags) {
	int running = 1;
	while (running) {
		// wait for user to press enter
		if (flags & FLAG_STEP) while (getchar() != '\n');
	
		cpu.MAR = cpu.PC++ & 0x0FFF;
		cpu.IR = mem[cpu.MAR];
		
		switch (cpu.IR >> 12) {
			case JNS: {
				cpu.MBR = cpu.PC;
				cpu.MAR = cpu.IR & 0x0FFF;
				mem[cpu.MAR] = cpu.MBR;
				cpu.MBR = cpu.IR & 0x0FFF;
				cpu.AC = ++cpu.MBR;
				cpu.PC = cpu.AC;
			} break;
			case LOAD: {
				cpu.MAR = cpu.IR & 0x0FFF;
				cpu.MBR = mem[cpu.MAR];
				cpu.AC = cpu.MBR;
			} break;
			case STORE: {
				cpu.MAR = cpu.IR & 0x0FFF;
				cpu.MBR = cpu.AC;
				mem[cpu.MAR] = cpu.MBR;
			} break;
			case ADD: {
				cpu.MAR = cpu.IR & 0x0FFF;
				cpu.MBR = mem[cpu.MAR];
				cpu.AC += cpu.MBR;
			} break;
			case SUBT: {
				cpu.MAR = cpu.IR & 0x0FFF;
				cpu.MBR = mem[cpu.MAR];
				cpu.AC -= cpu.MBR;
			} break;
			case INPUT: {
				cpu.InREG = getchar();
				cpu.AC = cpu.InREG;
			} break;
			case OUTPUT: {
				cpu.OutREG = cpu.AC;
				putchar(cpu.OutREG);
			} break;
			case HALT: {
				running = 0;
			} break;
			case SKIPCOND: {
				switch ((cpu.IR & 0xC00) >> 10) { // bits 10 and 11 of MAR
					case 0: if (cpu.AC < 0) ++cpu.PC; break;
					case 1: if (cpu.AC == 0) ++cpu.PC; break;
					case 2: if (cpu.AC > 0) ++cpu.PC;
				}
			} break;
			case JUMP: {
				cpu.PC = cpu.IR & 0x0FFF;
			} break;
			case CLEAR: {
				cpu.AC = 0;
			} break;
			case ADDI: {
				cpu.MAR = cpu.IR & 0x0FFF;
				cpu.MBR = mem[cpu.MAR];
				cpu.MAR = cpu.MBR;
				cpu.MBR = mem[cpu.MAR];
				cpu.AC += cpu.MBR;
			} break;
			case JUMPI: {
				cpu.MAR = cpu.IR & 0x0FFF;
				cpu.MBR = mem[cpu.MAR];
				cpu.PC = cpu.MBR;
			} break;
			case LOADI: {
				cpu.MAR = cpu.IR & 0x0FFF;
				cpu.MBR = mem[cpu.MAR];
				cpu.MAR = cpu.MBR;
				cpu.MBR = mem[cpu.MAR];
				cpu.AC = cpu.MBR;
			} break;
			case STOREI: {
				cpu.MAR = cpu.IR & 0x0FFF;
				cpu.MBR = mem[cpu.MAR];
				cpu.MAR = cpu.MBR;
				cpu.MBR = cpu.AC;
				mem[cpu.MAR] = cpu.MBR;
			} break;
			default: {
				fprintf(stderr, "Invalid instruction\n");
				exit(1);
			}
		}
		if (flags & FLAG_REGS) print_cpu_state();
	}
}

int main(int argc, char *argv[]) {
	char *path = NULL;
	unsigned int flags = 0;	
	for (char **arg = argv + 1; *arg; ++arg) {
		if (!strcmp(*arg, "-h") || !strcmp(*arg, "--help")) {
			printf(HELP, argv[0]);
			return 0;
		} else if (!strcmp(*arg, "-r") || !strcmp(*arg, "--regs")) {
			flags |= FLAG_REGS;
		} else if (!strcmp(*arg, "-s") || !strcmp(*arg, "--step")) {
			flags |= FLAG_STEP;
		} else {
			path = *arg;
			break;
		}
	}
	
	if (!path) {
		fputs("No executable file provided\n", stderr);
		printf(HELP, argv[0]);
		return 1;
	}
	
	FILE *fp = fopen(path, "r");
	
	if (!fp) {
		fprintf(stderr, "Error opening %s: %s\n", path, strerror(errno));
		return 1;
	}
	
	// get filesize
	fseek(fp, 0, SEEK_END);
	long prog_len = ftell(fp) / sizeof(uint16_t);
	rewind(fp);
	if (prog_len > 0x1000) {
		fprintf(stderr, "%s is an invalid executable. It is too large. Maximum size is 4096 words or 8192 bytes. (Program size is %ld words.)\n", path, prog_len);
		fclose(fp);
		return 1;
	}
	
	if (fread(mem, sizeof(uint16_t), prog_len, fp) != prog_len) {
		fputs("Failed to write program into memory.\n", stderr);
		fclose(fp);
		return 1;
	}
	
	fclose(fp);
	
	cpu.PC = 0x100;
	execute(flags);
	
	return 0;
}
