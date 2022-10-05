#ifndef MARIE_SHARED_H
#define MARIE_SHARED_H

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

enum time_unit {
	TIME_MICRO,
	TIME_MILLI,
	TIME_SEC
};

static void print_exec_time(clock_t exec_time) {
	const char *TIME_UNITS[] = {"\u03BCs", "ms", "s"};
	
	exec_time /= CLOCKS_PER_SEC / 1000000.0;
	
	enum time_unit unit;
	int exec_time_mod;
	if (exec_time % 1000 == exec_time) {
		unit = TIME_MICRO;
		exec_time_mod = 1;
	} else if (exec_time % 1000000 == exec_time) {
		unit = TIME_MILLI;
		exec_time_mod = 1000;
	} else {
		unit = TIME_SEC;
		exec_time_mod = 1000000;
	}
	
	printf("Time: %ld.%ld%s\n", exec_time / exec_time_mod, exec_time % exec_time_mod, TIME_UNITS[unit]);
}

#endif
