#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "../include/interface.h"

#define PASS(msg) printf("[PASS] %s\n", msg)
#define FAIL(msg) printf("[FAIL] %s\n", msg)

static int test_read_file_missing(void) {
    char *r = input_read_file("__nonexistent__.sql");
    if (r != NULL) { FAIL("read_file: missing file should return NULL"); free(r); return 1; }
    PASS("read_file: missing file returns NULL");
    return 0;
}

static int test_tokenize_select_star(void) {
    TokenList *list = lexer_tokenize("SELECT * FROM users");
    assert(list != NULL);
    assert(list->count >= 4);
    assert(list->tokens[0].type == TOKEN_SELECT);
    assert(list->tokens[1].type == TOKEN_STAR);
    assert(list->tokens[2].type == TOKEN_FROM);
    assert(list->tokens[3].type == TOKEN_IDENT);
    assert(strcmp(list->tokens[3].value, "users") == 0);
    lexer_free(list);
    PASS("tokenize: SELECT * FROM users -> 4 tokens");
    return 0;
}

static int test_tokenize_insert(void) {
    TokenList *list = lexer_tokenize("INSERT INTO users VALUES (1, 'alice')");
    assert(list != NULL);
    assert(list->tokens[0].type == TOKEN_INSERT);
    assert(list->tokens[1].type == TOKEN_INTO);
    lexer_free(list);
    PASS("tokenize: INSERT INTO ... first tokens correct");
    return 0;
}

static int test_tokenize_ends_with_eof(void) {
    TokenList *list = lexer_tokenize("SELECT * FROM t");
    assert(list != NULL);
    assert(list->tokens[list->count - 1].type == TOKEN_EOF);
    lexer_free(list);
    PASS("tokenize: last token is always TOKEN_EOF");
    return 0;
}

static int test_tokenize_empty(void) {
    TokenList *list = lexer_tokenize("");
    assert(list != NULL);
    assert(list->count == 1);
    assert(list->tokens[0].type == TOKEN_EOF);
    lexer_free(list);
    PASS("tokenize: empty string -> EOF only");
    return 0;
}

int main(void) {
    int fail = 0;
    printf("=== test_input ===\n");
    fail += test_read_file_missing();
    fail += test_tokenize_select_star();
    fail += test_tokenize_insert();
    fail += test_tokenize_ends_with_eof();
    fail += test_tokenize_empty();
    printf("=== %s ===\n", fail == 0 ? "ALL PASS" : "FAILED");
    return fail;
}
