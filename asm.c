// Required for strdup in Linux
#define _GNU_SOURCE
// Required to use strdup in Windows
#define _CRT_NONSTDC_NO_WARNINGS

#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include <errno.h>
#include <time.h>

#include "shared.h"

struct Symbol {
	char *key;
	uint16_t addr;
};

enum Directive {ORG};
enum Data {HEX, DEC};

static const char *DATA_SPEC[] = {
	"Hex", "Dec", NULL
};

static const bool INST_HAS_OP[] = {
	true, true, true, true, true, false, false, false,
	true, true, false, true, true, true, true
};

static const char *HELP = "Usage: %s [infile] [OPTIONS] -o [outfile]\n\t-t\tDisplay time to assemble (in \u03BCs)\n";

static struct Symbol symbols[4096];
static size_t num_symbols = 0;

static uint16_t program[4096] = {0};

static struct Symbol operands[4096];
static size_t num_operands = 0;

static int index_table(char *str, const char *table[]) {
	for (int i = 0; table[i]; ++i)
		if (!strcmp(str, table[i])) return i;
	
	return -1;
}

static int symbol_cmp(const void *p1, const void *p2) {
	return strcmp(((struct Symbol *) p1)->key, ((struct Symbol *) p2)->key);
}

static inline void add_symbol(char *str, uint16_t addr) {
	if (num_symbols > 0) {
		if (strcmp(symbols[num_symbols - 1].key, str) < 0) {
			symbols[num_symbols].key = strdup(str);
			symbols[num_symbols].addr = addr;
		} else {
			for (struct Symbol *s = symbols; s < symbols + num_symbols; ++s) {
				if (strcmp(s->key, str) >= 0) {
					// Shift table to the right
					memmove(s + 1, s, (symbols + num_symbols - s) * sizeof(struct Symbol));
					s->key = strdup(str);
					s->addr = addr;
					break;
				}
			}
		}
	} else if (num_symbols == 0) {
		symbols[0].key = strdup(str);
		symbols[0].addr = addr;
	}

	++num_symbols;
}

static inline void add_operand(char *str, uint16_t addr) {
	operands[num_operands++] = (struct Symbol) {
		.key = strdup(str),
		.addr = addr
	};
}

static inline int lookup_symbol(char *sym) {
	struct Symbol tmp = {.key = sym};
	struct Symbol *res = bsearch(
		&tmp, symbols, num_symbols, sizeof(struct Symbol), &symbol_cmp
	);
	
	return res ? res->addr : -1;
}

#define TRIM_LEADING_WHITESPACE(str) for (; isblank(*(str)); ++(str))

static inline void assemble(const char *in_path, const char *out_path) {
	char line[256]; // I hope your symbols aren't this long.
	char *str = line; // create pointer to more easily mutate the string
	
	FILE *fp = fopen(in_path, "r");
	if (!fp) {
		fprintf(stderr, "Error opening %s: %s\n", in_path, strerror(errno));
		exit(1);
	}
	
	size_t line_num;
	uint16_t addr;
	for (line_num = 0, addr = 0; fgets(line, 256, fp); ++line_num, ++addr) {
		// Crash if address is too large
		if (addr > 0xFFF) {
			fputs("Encountered large address. Maximum address is 0xFFF, your program is too large.\n", stderr);
			exit(1);
		}
	
		str = line; // reset pointer to the beginning of the line buffer		
		TRIM_LEADING_WHITESPACE(str);
		if (!*str || *str == '/' || *str == '\r' || *str == '\n') { // empty line
			--addr;
		} else {
			char *comment = strchr(str, '/');
			if (comment) *comment = '\0';
			else {
				size_t len = strlen(str);
				// remove newline characters
				if (len > 1 && str[len - 2] == '\r' || str[len - 2] == '\n') str[len - 2] = '\0';
				else if (len > 0 && str[len - 1] == '\n' || str[len - 1] == '\r') str[len - 1] = '\0';
			}
		
			// Find the end of a symbol if there is one and catalog it.
			char *symbol_end = strchr(str, ',');
			if (symbol_end) {
				*symbol_end = '\0';
				add_symbol(str, addr);
				str = symbol_end + 1;
				TRIM_LEADING_WHITESPACE(str);
			}
			
			size_t data;
			strtok(str, " \t"); // delimit at whitespace
			if (!strcmp(str, "ORG")) {
				addr = strtoul(str + 4, NULL, 16) - 1;
			} else if (!strcmp(str, "END")) {
				goto end;
			} else if ((data = index_table(str, DATA_SPEC)) != -1) {
				program[addr] = strtoul(str + 4, NULL, data ? 10 : 16);
			} else if ((data = index_table(str, INSTRUCTIONS)) != -1) {
				program[addr] = data << 12;
				/* Parsing of every operand must be deferred until after
				 * the symbol table is populated because it is ambiguous
				 * whether an operand is a symbol or a hexadecimal address. */
				if (INST_HAS_OP[data]) {
					str = strtok(NULL, " \t");
					if (!*str) {
						++str;
						TRIM_LEADING_WHITESPACE(str);
						strtok(str, " \t");
					}
					add_operand(str, addr);
				}
			} else {
				fprintf(stderr, "%s:%zu: error: invalid instruction\n%s\n", in_path, line_num, str);
				exit(1);
			}
		}
	}
	
	end:
	// Insert operands into program
	for (struct Symbol *s = operands; s < operands + num_operands; ++s) {
		int sym = lookup_symbol(s->key);
		program[s->addr] |= sym == -1 ? strtoul(s->key, NULL, 16) : sym;
	}
	
	// Replace file descriptor for input with output, and write the file.
	fclose(fp);
	fp = fopen(out_path, "w");
	if (!fp) {
		fprintf(stderr, "Error opening %s: %s\n", out_path, strerror(errno));
		exit(1);
	}
	
	fwrite(program, sizeof(uint16_t), addr, fp);
	fclose(fp);
}

static char *change_ext(const char *path) {
	// find .
	size_t len = strlen(path);
	const char *path_end = path + len;
	while (*path_end != '.' && path_end != path) --path_end;
	
	// find lengths of things
	size_t len_no_ext = path == path_end ? len : path_end - path;
	size_t new_len = len_no_ext + 5;
	
	// create new string
	char *new_path = malloc(new_len + 1);
	memcpy(new_path, path, len_no_ext);
	strcpy(new_path + len_no_ext, ".mex2");
	
	return new_path;
}

int main(int argc, char *argv[]) {
	char *in_path = NULL, *out_path = NULL;
	bool profile_time = false;
	for (char **arg = argv + 1; *arg; ++arg) {
		if (!strcmp(*arg, "-t")) profile_time = true; 
		else if (!strcmp(*arg, "-o")) {
			if (!*++arg) {
				puts("No output file provided.");
				return 1;
			} else out_path = *arg;
		} else in_path = *arg;
	}
	
	if (!in_path) {
		printf(HELP, argv[0]);
		return 1;
	}
	
	bool generated_path = false;
	if (!out_path) {
		generated_path = true;
		out_path = change_ext(in_path);
	}
	
	clock_t start = clock();
	assemble(in_path, out_path);
	clock_t end = clock();
	
	if (profile_time) print_exec_time(end - start);
	
	return 0;
}
