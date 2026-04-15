#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../../include/interface.h"

/* =========================================================
 * Parser implementation for the AST interface in interface.h.
 * Supported grammar:
 *   SELECT * FROM <table>
 *   SELECT <col1>, <col2>, ... FROM <table>
 *   SELECT * FROM <table> WHERE <col> = <val>
 *   INSERT INTO <table> VALUES (<val1>, <val2>, ...)
 *   INSERT INTO <table> (<col1>, <col2>, ...) VALUES (<val1>, <val2>, ...)
 * ========================================================= */

/* Return the token at the current position or EOF when out of range. */
static Token *peek(TokenList *tokens, int pos) {
    if (pos >= tokens->count) return &tokens->tokens[tokens->count - 1];
    return &tokens->tokens[pos];
}

/* Consume an expected token and advance the cursor on success. */
static int expect(TokenList *tokens, int *pos, TokenType type) {
    Token *token = peek(tokens, *pos);

    if (token->type != type) {
        fprintf(stderr, "parse error: unexpected token '%s' at line %d\n",
                token->value, token->line);
        return SQL_ERR;
    }

    (*pos)++;
    return SQL_OK;
}

static char *dup_value(const char *value) {
    size_t len = strlen(value) + 1;
    char *copy = calloc(len, sizeof(char));

    if (!copy) {
        fprintf(stderr, "parser: out of memory\n");
        return NULL;
    }

    memcpy(copy, value, len);
    return copy;
}

static void copy_text(char *dst, size_t dst_size, const char *src) {
    size_t len;

    if (!dst || dst_size == 0) return;
    if (!src) {
        dst[0] = '\0';
        return;
    }

    len = strlen(src);
    if (len >= dst_size) len = dst_size - 1;

    memcpy(dst, src, len);
    dst[len] = '\0';
}

static int is_value_token(TokenType type) {
    return type == TOKEN_IDENT || type == TOKEN_STRING || type == TOKEN_INTEGER;
}

static int append_string(char ***items, int *count, int *capacity, const char *value) {
    if (*count == *capacity) {
        int new_capacity = (*capacity == 0) ? 4 : (*capacity * 2);
        char **resized = realloc(*items, (size_t)new_capacity * sizeof(char *));

        if (!resized) {
            fprintf(stderr, "parser: out of memory\n");
            return SQL_ERR;
        }

        *items = resized;
        *capacity = new_capacity;
    }

    (*items)[*count] = dup_value(value);
    if (!(*items)[*count]) return SQL_ERR;

    (*count)++;
    return SQL_OK;
}

static int expect_ident(TokenList *tokens, int *pos, char *dst, size_t dst_size) {
    Token *token = peek(tokens, *pos);

    if (token->type != TOKEN_IDENT) {
        fprintf(stderr, "parse error: unexpected token '%s' at line %d\n",
                token->value, token->line);
        return SQL_ERR;
    }

    copy_text(dst, dst_size, token->value);
    (*pos)++;
    return SQL_OK;
}

static int expect_value(TokenList *tokens, int *pos, char *dst, size_t dst_size) {
    Token *token = peek(tokens, *pos);

    if (!is_value_token(token->type)) {
        fprintf(stderr, "parse error: unexpected token '%s' at line %d\n",
                token->value, token->line);
        return SQL_ERR;
    }

    copy_text(dst, dst_size, token->value);
    (*pos)++;
    return SQL_OK;
}

static int parse_ident_list(TokenList *tokens,
                            int *pos,
                            char ***items,
                            int *count,
                            int *capacity) {
    if (expect(tokens, pos, TOKEN_LPAREN) != SQL_OK) return SQL_ERR;

    while (1) {
        Token *token = peek(tokens, *pos);

        if (token->type != TOKEN_IDENT) {
            fprintf(stderr, "parse error: unexpected token '%s' at line %d\n",
                    token->value, token->line);
            return SQL_ERR;
        }

        if (append_string(items, count, capacity, token->value) != SQL_OK) return SQL_ERR;

        (*pos)++;
        if (peek(tokens, *pos)->type != TOKEN_COMMA) break;
        (*pos)++;
    }

    return expect(tokens, pos, TOKEN_RPAREN);
}

static int parse_value_list(TokenList *tokens,
                            int *pos,
                            char ***items,
                            int *count,
                            int *capacity) {
    if (expect(tokens, pos, TOKEN_LPAREN) != SQL_OK) return SQL_ERR;

    while (1) {
        Token *token = peek(tokens, *pos);

        if (!is_value_token(token->type)) {
            fprintf(stderr, "parse error: unexpected token '%s' at line %d\n",
                    token->value, token->line);
            return SQL_ERR;
        }

        if (append_string(items, count, capacity, token->value) != SQL_OK) return SQL_ERR;

        (*pos)++;
        if (peek(tokens, *pos)->type != TOKEN_COMMA) break;
        (*pos)++;
    }

    return expect(tokens, pos, TOKEN_RPAREN);
}

static ASTNode *parse_select(TokenList *tokens) {
    int pos = 0;
    int capacity = 0;
    ASTNode *node = calloc(1, sizeof(ASTNode));

    if (!node) {
        fprintf(stderr, "parser: out of memory\n");
        return NULL;
    }

    node->type = STMT_SELECT;

    if (expect(tokens, &pos, TOKEN_SELECT) != SQL_OK) goto fail;

    if (peek(tokens, pos)->type == TOKEN_STAR) {
        node->select.select_all = 1;
        pos++;
    } else {
        while (1) {
            Token *column = peek(tokens, pos);

            if (column->type != TOKEN_IDENT) {
                fprintf(stderr, "parse error: unexpected token '%s' at line %d\n",
                        column->value, column->line);
                goto fail;
            }

            if (append_string(&node->select.columns,
                              &node->select.column_count,
                              &capacity,
                              column->value) != SQL_OK) {
                goto fail;
            }

            pos++;
            if (peek(tokens, pos)->type != TOKEN_COMMA) break;
            pos++;
        }
    }

    if (expect(tokens, &pos, TOKEN_FROM) != SQL_OK) goto fail;
    if (expect_ident(tokens, &pos, node->select.table, sizeof(node->select.table)) != SQL_OK)
        goto fail;

    if (peek(tokens, pos)->type == TOKEN_WHERE) {
        pos++;
        node->select.has_where = 1;

        if (expect_ident(tokens, &pos,
                         node->select.where.col,
                         sizeof(node->select.where.col)) != SQL_OK) {
            goto fail;
        }

        if (expect(tokens, &pos, TOKEN_EQ) != SQL_OK) goto fail;
        if (expect_value(tokens, &pos,
                         node->select.where.val,
                         sizeof(node->select.where.val)) != SQL_OK) {
            goto fail;
        }
    }

    if (expect(tokens, &pos, TOKEN_EOF) != SQL_OK) goto fail;
    return node;

fail:
    parser_free(node);
    return NULL;
}

static ASTNode *parse_insert(TokenList *tokens) {
    int pos = 0;
    int column_capacity = 0;
    int value_capacity = 0;
    ASTNode *node = calloc(1, sizeof(ASTNode));

    if (!node) {
        fprintf(stderr, "parser: out of memory\n");
        return NULL;
    }

    node->type = STMT_INSERT;

    if (expect(tokens, &pos, TOKEN_INSERT) != SQL_OK) goto fail;
    if (expect(tokens, &pos, TOKEN_INTO) != SQL_OK) goto fail;
    if (expect_ident(tokens, &pos, node->insert.table, sizeof(node->insert.table)) != SQL_OK)
        goto fail;

    if (peek(tokens, pos)->type == TOKEN_LPAREN) {
        if (parse_ident_list(tokens,
                             &pos,
                             &node->insert.columns,
                             &node->insert.column_count,
                             &column_capacity) != SQL_OK) {
            goto fail;
        }
    }

    if (expect(tokens, &pos, TOKEN_VALUES) != SQL_OK) goto fail;
    if (parse_value_list(tokens,
                         &pos,
                         &node->insert.values,
                         &node->insert.value_count,
                         &value_capacity) != SQL_OK) {
        goto fail;
    }
    if (expect(tokens, &pos, TOKEN_EOF) != SQL_OK) goto fail;
    return node;

fail:
    parser_free(node);
    return NULL;
}

ASTNode *parser_parse(TokenList *tokens) {
    if (!tokens || tokens->count == 0) return NULL;

    switch (peek(tokens, 0)->type) {
        case TOKEN_SELECT:
            return parse_select(tokens);
        case TOKEN_INSERT:
            return parse_insert(tokens);
        default: {
            Token *token = peek(tokens, 0);
            fprintf(stderr, "parse error: unexpected token '%s' at line %d\n",
                    token->value, token->line);
            return NULL;
        }
    }
}

void parser_free(ASTNode *node) {
    if (!node) return;

    if (node->type == STMT_SELECT) {
        if (node->select.columns) {
            for (int i = 0; i < node->select.column_count; i++)
                free(node->select.columns[i]);
            free(node->select.columns);
        }
    } else if (node->type == STMT_INSERT) {
        if (node->insert.columns) {
            for (int i = 0; i < node->insert.column_count; i++)
                free(node->insert.columns[i]);
            free(node->insert.columns);
        }

        if (node->insert.values) {
            for (int i = 0; i < node->insert.value_count; i++)
                free(node->insert.values[i]);
            free(node->insert.values);
        }
    }

    free(node);
}
