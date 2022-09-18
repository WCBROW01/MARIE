#ifndef INSTRUCTIONS_H
#define INSTRUCTIONS_H

enum instruction {
	JNS,
	LOAD,
	STORE,
	ADD,
	SUBT,
	INPUT,
	OUTPUT,
	HALT,
	SKIPCOND,
	JUMP,
	CLEAR,
	ADDI,
	JUMPI,
	LOADI,
	STOREI
};

static const char *INSTRUCTIONS[] = {
	"JnS", "Load", "Store", "Add", "Subt", "Input", "Output", "Halt",
	"Skipcond", "Jump", "Clear", "AddI", "JumpI", "LoadI", "StoreI", NULL
};

#endif
