#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include <errno.h>

#include "cvector.h"
#include "instructions.h"

#ifndef MARIE_DEBUG
#define MARIE_DEBUG 0
#endif

// TODO: Assemble instructions and directives before 2nd pass
// rather than keeping them as strings (to save memory and execution time)

struct Symbol {
	char *key;
	uint16_t addr;
};

enum TokenType {
	DIRECTIVE, DATA, INST, VALUE, NEXT
};

struct Token {
	char *token;
	enum TokenType type;
};

static const char *DATA_SPEC[] = {
	"Hex", "Dec", NULL
};

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

static void add_token(Vec *tokens, char *token, enum TokenType type) {
	struct Token tok = {
		.token = token ? strdup(token) : NULL,
		.type = type
	};
	
	Vec_push(tokens, &tok);
}

#define TRIM_LEADING_WHITESPACE(str) for (; isblank(*(str)); ++(str))

static Vec *tokenize(FILE *fp, Vec *symbols) {
	char line[256]; // I hope your symbols aren't this long.
	char *str = line; // create pointer to more easily mutate the string
	Vec *tokens = Vec(struct Token);
	
	for (uint16_t addr = 0; fgets(line, 256, fp); ++addr) {
		// Crash if address is too large
		if (addr > 0xFFF) {
			fputs("Encountered large address. Maximum address is 0xFFF, your program is too large.\n", stderr);
			exit(1);
		}
	
		str = line; // reset pointer to the beginning pf the line buffer		
		TRIM_LEADING_WHITESPACE(str);
		strtok(str, "/"); // Remove comments
		strtok(str, "\r\n"); // remove newline characters
		strtok(NULL, "\r\n");
		
		// empty line
		if (!*str || *str == '/' || *str == '\r' || *str == '\n') --addr;
		else {
			// Find the end of a symbol if there is one and catalog it.
			char *symbol_end = strchr(str, ',');
			if (symbol_end) {
				*symbol_end = '\0';
				add_symbol(symbols, str, addr);
				str = symbol_end + 1;
			}
			
			while (*str) {
				TRIM_LEADING_WHITESPACE(str);
				
				char *token_end;
				strtok_r(str, " \t", &token_end); // delimit at whitespace
				
				if (!strcmp(str, "ORG")) {
					addr = strtoul(str + 4, NULL, 16) - 1;
					add_token(tokens, "ORG", DIRECTIVE);
				} else if (!strcmp(str, "END")) {
					add_token(tokens, "NEXT", NEXT);
					goto end; // forcefully end
				} else if (index_table(str, DATA_SPEC) != -1) {
					add_token(tokens, str, DATA);
				} else if (index_table(str, INSTRUCTIONS) != -1) {
					add_token(tokens, str, INST);
				} else {
					add_token(tokens, str, VALUE);
				}
				
				str = token_end;
				// Make sure loop actually ends if we're done
				TRIM_LEADING_WHITESPACE(str);
			}
			
			add_token(tokens, "NEXT", NEXT);
		}
	}
	
	end:
	// sort symbol list
	qsort(symbols->data, symbols->len, sizeof(struct Symbol), &symbol_cmp);
	return tokens;
}

static void free_tokens(void *ptr) {
	free(((struct Token *) ptr)->token);
}

static void free_symbols(void *ptr) {
	free(((struct Symbol *) ptr)->key);
}

#if MARIE_DEBUG
static void print_tokens(Vec *tokens) {\
	for (
		struct Token *tok = tokens->data;
		tok < (struct Token *) tokens->data + tokens->len;
		++tok
	) printf("Token: \"%s\", Type: %d\n", tok->token, tok->type);
}
#endif

static void assemble(Vec *symbols, Vec *tokens, const char *path) {
	uint16_t program[4096];
	uint16_t addr = 0;
	uint16_t word = 0;
	int radix = 16;
	
	for (
		struct Token *tok = tokens->data;
		tok < (struct Token *) tokens->data + tokens->len;
		++tok
	) {
		switch (tok->type) {
			case DIRECTIVE: {
				if (!strcmp(tok->token, "ORG") && (++tok)->type != VALUE) {
					fputs("Invalid use of \"ORG\" detected.\n", stderr);
					exit(1);
				} else addr = strtoul(tok->token, NULL, radix) - 1;
			} break;
			// If return value isn't zero, the only possibility is decimal.
			case DATA: if (index_table(tok->token, DATA_SPEC)) radix = 10; break;
			case INST: word |= index_table(tok->token, INSTRUCTIONS) << 12; break;
			case VALUE: {
				int lookup_res = lookup_symbol(symbols, tok->token);
				if (lookup_res != -1) word |= lookup_res;
				else word |= strtoul(tok->token, NULL, radix);
				radix = 16;
			} break;
			case NEXT: {
				program[addr++] = word;
				word = 0;
			}
		}
	}
	
	FILE *fp = fopen(path, "w");
	fwrite(program, sizeof(uint16_t), addr, fp);
	fclose(fp);
}

int main(int argc, char *argv[]) {
	if (!argv[1]) {
		fputs("No assembly file provided\n", stderr);
		return 1;
	}
	
	char *path = argv[1];
	FILE *fp = fopen(path, "r");
	if (!fp) {
		fprintf(stderr, "Error opening %s: %s\n", path, strerror(errno));
		return 1;
	}
	
	Vec *symbols = Vec(struct Symbol);
	Vec *tokens = tokenize(fp, symbols);
	fclose(fp);
#if MARIE_DEBUG
	print_tokens(tokens);
#endif
	assemble(symbols, tokens, "a.out");
	
	// free resources
	Vec_destroy(tokens, &free_tokens);
	Vec_destroy(symbols, &free_symbols);
	return 0;
}
