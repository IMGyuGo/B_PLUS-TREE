#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "../include/interface.h"

#define PASS(msg) printf("[PASS] %s\n", msg)

static Token make_token(TokenType type, const char *value) {
    Token token;

    memset(&token, 0, sizeof(token));
    token.type = type;
    token.line = 1;
    if (value) {
        strncpy(token.value, value, sizeof(token.value) - 1);
        token.value[sizeof(token.value) - 1] = '\0';
    }

    return token;
}

static ASTNode *parse_tokens(const Token *src, int count) {
    TokenList *tokens = calloc(1, sizeof(TokenList));
    assert(tokens != NULL);

    tokens->tokens = calloc((size_t)count, sizeof(Token));
    assert(tokens->tokens != NULL);
    memcpy(tokens->tokens, src, (size_t)count * sizeof(Token));
    tokens->count = count;

    ASTNode *ast = parser_parse(tokens);
    free(tokens->tokens);
    free(tokens);
    return ast;
}

static int test_select_star(void) {
    Token tokens[] = {
        make_token(TOKEN_SELECT, "SELECT"),
        make_token(TOKEN_STAR, "*"),
        make_token(TOKEN_FROM, "FROM"),
        make_token(TOKEN_IDENT, "users"),
        make_token(TOKEN_EOF, "")
    };
    ASTNode *ast = parse_tokens(tokens, (int)(sizeof(tokens) / sizeof(tokens[0])));
    assert(ast != NULL);
    assert(ast->type == STMT_SELECT);
    assert(ast->select.select_all == 1);
    assert(strcmp(ast->select.table, "users") == 0);
    parser_free(ast);
    PASS("parse: SELECT * FROM users");
    return 0;
}

static int test_select_columns(void) {
    Token tokens[] = {
        make_token(TOKEN_SELECT, "SELECT"),
        make_token(TOKEN_IDENT, "id"),
        make_token(TOKEN_COMMA, ","),
        make_token(TOKEN_IDENT, "name"),
        make_token(TOKEN_FROM, "FROM"),
        make_token(TOKEN_IDENT, "users"),
        make_token(TOKEN_EOF, "")
    };
    ASTNode *ast = parse_tokens(tokens, (int)(sizeof(tokens) / sizeof(tokens[0])));
    assert(ast != NULL);
    assert(ast->type == STMT_SELECT);
    assert(ast->select.select_all == 0);
    assert(ast->select.column_count == 2);
    assert(strcmp(ast->select.columns[0], "id") == 0);
    assert(strcmp(ast->select.columns[1], "name") == 0);
    parser_free(ast);
    PASS("parse: SELECT id, name FROM users");
    return 0;
}

static int test_select_where(void) {
    Token tokens[] = {
        make_token(TOKEN_SELECT, "SELECT"),
        make_token(TOKEN_STAR, "*"),
        make_token(TOKEN_FROM, "FROM"),
        make_token(TOKEN_IDENT, "users"),
        make_token(TOKEN_WHERE, "WHERE"),
        make_token(TOKEN_IDENT, "id"),
        make_token(TOKEN_EQ, "="),
        make_token(TOKEN_INTEGER, "1"),
        make_token(TOKEN_EOF, "")
    };
    ASTNode *ast = parse_tokens(tokens, (int)(sizeof(tokens) / sizeof(tokens[0])));
    assert(ast != NULL);
    assert(ast->select.has_where == 1);
    assert(strcmp(ast->select.where.col, "id") == 0);
    assert(strcmp(ast->select.where.val, "1") == 0);
    parser_free(ast);
    PASS("parse: SELECT * FROM users WHERE id = 1");
    return 0;
}

static int test_insert(void) {
    Token tokens[] = {
        make_token(TOKEN_INSERT, "INSERT"),
        make_token(TOKEN_INTO, "INTO"),
        make_token(TOKEN_IDENT, "users"),
        make_token(TOKEN_VALUES, "VALUES"),
        make_token(TOKEN_LPAREN, "("),
        make_token(TOKEN_INTEGER, "1"),
        make_token(TOKEN_COMMA, ","),
        make_token(TOKEN_STRING, "alice"),
        make_token(TOKEN_COMMA, ","),
        make_token(TOKEN_INTEGER, "30"),
        make_token(TOKEN_RPAREN, ")"),
        make_token(TOKEN_EOF, "")
    };
    ASTNode *ast = parse_tokens(tokens, (int)(sizeof(tokens) / sizeof(tokens[0])));
    assert(ast != NULL);
    assert(ast->type == STMT_INSERT);
    assert(strcmp(ast->insert.table, "users") == 0);
    assert(ast->insert.value_count == 3);
    assert(strcmp(ast->insert.values[0], "1") == 0);
    assert(strcmp(ast->insert.values[1], "alice") == 0);
    assert(strcmp(ast->insert.values[2], "30") == 0);
    parser_free(ast);
    PASS("parse: INSERT INTO users VALUES (1, 'alice', 30)");
    return 0;
}

static int test_insert_with_columns(void) {
    Token tokens[] = {
        make_token(TOKEN_INSERT, "INSERT"),
        make_token(TOKEN_INTO, "INTO"),
        make_token(TOKEN_IDENT, "users"),
        make_token(TOKEN_LPAREN, "("),
        make_token(TOKEN_IDENT, "id"),
        make_token(TOKEN_COMMA, ","),
        make_token(TOKEN_IDENT, "name"),
        make_token(TOKEN_RPAREN, ")"),
        make_token(TOKEN_VALUES, "VALUES"),
        make_token(TOKEN_LPAREN, "("),
        make_token(TOKEN_INTEGER, "1"),
        make_token(TOKEN_COMMA, ","),
        make_token(TOKEN_STRING, "alice"),
        make_token(TOKEN_RPAREN, ")"),
        make_token(TOKEN_EOF, "")
    };
    ASTNode *ast = parse_tokens(tokens, (int)(sizeof(tokens) / sizeof(tokens[0])));
    assert(ast != NULL);
    assert(ast->type == STMT_INSERT);
    assert(strcmp(ast->insert.table, "users") == 0);
    assert(ast->insert.column_count == 2);
    assert(strcmp(ast->insert.columns[0], "id") == 0);
    assert(strcmp(ast->insert.columns[1], "name") == 0);
    assert(ast->insert.value_count == 2);
    assert(strcmp(ast->insert.values[0], "1") == 0);
    assert(strcmp(ast->insert.values[1], "alice") == 0);
    parser_free(ast);
    PASS("parse: INSERT INTO users (id, name) VALUES (1, 'alice')");
    return 0;
}

static int test_invalid_returns_null(void) {
    Token tokens[] = {
        make_token(TOKEN_IDENT, "GARBAGE"),
        make_token(TOKEN_IDENT, "SQL"),
        make_token(TOKEN_EOF, "")
    };
    ASTNode *ast = parse_tokens(tokens, (int)(sizeof(tokens) / sizeof(tokens[0])));
    assert(ast == NULL);
    PASS("parse: invalid SQL returns NULL");
    return 0;
}

int main(void) {
    int fail = 0;
    printf("=== test_parser ===\n");
    fail += test_select_star();
    fail += test_select_columns();
    fail += test_select_where();
    fail += test_insert();
    fail += test_insert_with_columns();
    fail += test_invalid_returns_null();
    printf("=== %s ===\n", fail == 0 ? "ALL PASS" : "FAILED");
    return fail;
}
