#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "../include/interface.h"

#define PASS(msg) printf("[PASS] %s\n", msg)

static int test_load_users(void) {
    TableSchema *s = schema_load("users");
    assert(s != NULL);
    assert(strcmp(s->table_name, "users") == 0);
    assert(s->column_count == 4);
    assert(s->columns[3].type == COL_BOOLEAN);
    schema_free(s);
    PASS("schema_load: users -> 4 columns (incl. BOOLEAN)");
    return 0;
}

static int test_load_missing(void) {
    TableSchema *s = schema_load("__no_such_table__");
    assert(s == NULL);
    PASS("schema_load: missing table returns NULL");
    return 0;
}

static int test_validate_insert_ok(void) {
    /* INSERT INTO users VALUES (1, 'alice', 'male', 'T') */
    ASTNode node = {0};
    node.type = STMT_INSERT;
    strncpy(node.insert.table, "users", sizeof(node.insert.table));
    char *vals[] = {"1", "alice", "male", "T"};
    node.insert.values = vals;
    node.insert.value_count = 4;

    TableSchema *s = schema_load("users");
    assert(s != NULL);
    assert(schema_validate(&node, s) == SQL_OK);
    schema_free(s);
    PASS("schema_validate: INSERT with 4 values -> SQL_OK");
    return 0;
}

static int test_validate_insert_wrong_count(void) {
    ASTNode node = {0};
    node.type = STMT_INSERT;
    strncpy(node.insert.table, "users", sizeof(node.insert.table));
    char *vals[] = {"1"};
    node.insert.values = vals;
    node.insert.value_count = 1;  /* 4개 필요한데 1개 */

    TableSchema *s = schema_load("users");
    assert(s != NULL);
    assert(schema_validate(&node, s) == SQL_ERR);
    schema_free(s);
    PASS("schema_validate: INSERT wrong count -> SQL_ERR");
    return 0;
}

static int test_validate_select_star(void) {
    ASTNode node;
    node.type = STMT_SELECT;
    node.select.select_all = 1;
    node.select.column_count = 0;
    node.select.columns = NULL;
    strncpy(node.select.table, "users", sizeof(node.select.table));
    node.select.has_where = 0;

    TableSchema *s = schema_load("users");
    assert(s != NULL);
    assert(schema_validate(&node, s) == SQL_OK);
    schema_free(s);
    PASS("schema_validate: SELECT * -> SQL_OK");
    return 0;
}

int main(void) {
    int fail = 0;
    printf("=== test_schema ===\n");
    fail += test_load_users();
    fail += test_load_missing();
    fail += test_validate_insert_ok();
    fail += test_validate_insert_wrong_count();
    fail += test_validate_select_star();
    printf("=== %s ===\n", fail == 0 ? "ALL PASS" : "FAILED");
    return fail;
}
