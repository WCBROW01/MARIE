#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include <errno.h>
#include <time.h>

#include "cvector.h"
#include "instructions.h"

#ifndef MARIE_DEBUG
#define MARIE_DEBUG 0
#endif

struct Symbol {
	char *key;
	uint16_t addr;
};

enum TokenType {DIRECTIVE, DATA, INST, VALUE, NEXT};
enum Directive {ORG};
enum Data {HEX, DEC};

struct Token {
	union {
		char *str;
		int data;
	} value;
	enum TokenType type;
};

static const char *DATA_SPEC[] = {
	"Hex", "Dec", NULL
};

static const char *HELP = "Usage: %s [infile] [OPTIONS] -o [outfile]\n\t-t\tDisplay time to assemble (in \u03BCs)\n";

static ssize_t index_table(char *str, const char *table[]) {
	for (ssize_t i = 0; table[i]; ++i)
		if (!strcmp(str, table[i])) return i;
	
	return -1;
}

static int symbol_cmp(const void *p1, const void *p2) {
	return strcmp(((struct Symbol *) p1)->key, ((struct Symbol *) p2)->key);
}

static inline void add_symbol(Vec *symbols, char *str, uint16_t addr) {
	struct Symbol newSym = {
		.key = strdup(str),
		.addr = addr
	};
	
	Vec_push(symbols, &newSym);
}

static int lookup_symbol(Vec *symbols, char *sym) {
	struct Symbol tmp = {.key = sym};
	struct Symbol *res = bsearch(
		&tmp, symbols->data, symbols->len, sizeof(struct Symbol), &symbol_cmp
	);
	
	return res ? res->addr : -1;
}

#if MARIE_DEBUG
static void print_token(struct Token *tok) {
	const char *str;
	switch (tok->type) {
		case DIRECTIVE: str = "ORG"; break;
		case INST: str = INSTRUCTIONS[tok->value.data]; break;
		case VALUE: str = tok->value.str; break;
		case NEXT: str = "NEXT"; break;
	}
	if (str) printf("Token: \"%s\", Type: %d\n", str, tok->type);
	else printf("Token: \"%x\", Type: %d\n", tok->value.data, tok->type);
}

static void print_tokens(Vec *tokens) {\
	for (
		struct Token *tok = tokens->data;
		tok < (struct Token *) tokens->data + tokens->len;
		++tok
	) print_token(tok);
}
#endif

#define TRIM_LEADING_WHITESPACE(str) for (; isblank(*(str)); ++(str))

static Vec *tokenize(FILE *fp, Vec *symbols) {
	char line[256]; // I hope your symbols aren't this long.
	char *str = line; // create pointer to more easily mutate the string
#if MARIE_DEBUG
	puts("Tokenizing");
#endif
	Vec *tokens = Vec(struct Token);
	
	for (uint16_t addr = 0; fgets(line, 256, fp); ++addr) {
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
				add_symbol(symbols, str, addr);
				str = symbol_end + 1;
				TRIM_LEADING_WHITESPACE(str);
			}
			
			struct Token token;
			strtok(str, " \t"); // delimit at whitespace
			while (str && *str) {
				if (!strcmp(str, "ORG")) {
					addr = strtoul(str + 4, NULL, 16) - 1;
					token.type = DIRECTIVE;
					token.value.data = ORG;
					Vec_push(tokens, &token);
				} else if (!strcmp(str, "END")) {
					token.type = NEXT;
					Vec_push(tokens, &token);
					goto end; // forcefully end
				} else if ((token.value.data = index_table(str, DATA_SPEC)) != -1) {
					token.type = DATA;
					Vec_push(tokens, &token);
				} else if ((token.value.data = index_table(str, INSTRUCTIONS)) != -1) {
					token.type = INST;
					Vec_push(tokens, &token);
				} else {
					token.type = VALUE;
					token.value.str = strdup(str);
					Vec_push(tokens, &token);
				}
				
				str = strtok(NULL, " \t");
				if (str) TRIM_LEADING_WHITESPACE(str);
			}
			
			token.type = NEXT;
			Vec_push(tokens, &token);
		}
	}
	
	end:
#if MARIE_DEBUG
	print_tokens(tokens);
#endif

	// sort symbol list
	qsort(symbols->data, symbols->len, sizeof(struct Symbol), &symbol_cmp);
	return tokens;
}

static void free_tokens(void *ptr) {
	struct Token *tok = ptr;
	if (tok->type == VALUE) free(tok->value.str);
}

static void free_symbols(void *ptr) {
	free(((struct Symbol *) ptr)->key);
}

static void assemble(Vec *symbols, Vec *tokens, const char *path) {
	uint16_t program[4096] = {0};
	uint16_t addr = 0;
	uint16_t word = 0;
	int radix = 16;
	bool org_changed = false;
	
#if MARIE_DEBUG
	puts("Assembling");
#endif
	
	for (
		struct Token *tok = tokens->data;
		tok < (struct Token *) tokens->data + tokens->len;
		++tok
	) {
#if MARIE_DEBUG
		print_token(tok);
#endif
		switch (tok->type) {
			case DIRECTIVE: {
				if ((++tok)->type != VALUE) {
					fputs("Invalid use of \"ORG\" detected.\n", stderr);
					exit(1);
				} else {
					uint16_t addr_old = addr;
					addr = strtoul(tok->value.str, NULL, 16);
					if (addr < addr_old) {
						fprintf(stderr, 
							"ORG used to jump to an address lower than the current one. Only in-order uses of ORG are valid.\n"
							"Old address: %x\nNew address: %x\n", addr_old, addr
						);
						exit(1);
					} else org_changed = true;
				}
			} break;
			// 0: hex, 1: dec
			case DATA: if (tok->value.data) radix = 10; break;
			case INST: word = tok->value.data << 12; break;
			case VALUE: {
				int lookup_res = lookup_symbol(symbols, tok->value.str);
				if (lookup_res != -1) word |= lookup_res;
				else word |= strtoul(tok->value.str, NULL, radix);
				radix = 16;
			} break;
			case NEXT: {
				if (!org_changed) program[addr++] = word;
				else org_changed = false;
				word = 0;
			}
		}
#if MARIE_DEBUG
		printf("Addr: %x, Word: %x\n", addr, word);
#endif		
	}
	
	FILE *fp = fopen(path, "w");
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
	FILE *fp = fopen(in_path, "r");
	if (!fp) {
		fprintf(stderr, "Error opening %s: %s\n", in_path, strerror(errno));
		return 1;
	}
	
	Vec *symbols = Vec(struct Symbol);
	Vec *tokens = tokenize(fp, symbols);
	fclose(fp);
	assemble(symbols, tokens, out_path);
	clock_t end = clock();
	
	if (profile_time)
		printf("Time: %ld\u03BCs\n", (end - start) / (CLOCKS_PER_SEC / 1000000));
	
	// free resources
	Vec_destroy(tokens, &free_tokens);
	Vec_destroy(symbols, &free_symbols);
	if (generated_path) free(out_path);
	return 0;
}
