#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "../include/interface.h"

#define PASS(msg) printf("[PASS] %s\n", msg)
#define TEST_TABLE "test_exec_tmp"
#define TEST_DAT   "data/" TEST_TABLE ".dat"

/* 테스트 전 임시 파일 제거 */
static void cleanup(void) { remove(TEST_DAT); }

static TableSchema *make_test_schema(void) {
    TableSchema *s = calloc(1, sizeof(TableSchema));
    strncpy(s->table_name, TEST_TABLE, sizeof(s->table_name));
    s->columns = calloc(2, sizeof(ColDef));
    s->column_count = 2;
    strncpy(s->columns[0].name, "id",   sizeof(s->columns[0].name));
    s->columns[0].type = COL_INT;
    strncpy(s->columns[1].name, "name", sizeof(s->columns[1].name));
    s->columns[1].type = COL_VARCHAR;
    s->columns[1].max_len = 64;
    return s;
}

static int test_insert_creates_file(void) {
    cleanup();
    TableSchema *s = make_test_schema();

    InsertStmt stmt = {0};
    strncpy(stmt.table, TEST_TABLE, sizeof(stmt.table));
    char *vals[] = {"1", "alice"};
    stmt.values = vals;
    stmt.value_count = 2;

    assert(db_insert(&stmt, s) == SQL_OK);

    FILE *fp = fopen(TEST_DAT, "r");
    assert(fp != NULL);
    fclose(fp);

    schema_free(s);
    cleanup();
    PASS("db_insert: creates file and returns SQL_OK");
    return 0;
}

static int test_select_empty_file(void) {
    cleanup();
    TableSchema *s = make_test_schema();

    SelectStmt stmt;
    stmt.select_all = 1;
    stmt.column_count = 0;
    stmt.columns = NULL;
    strncpy(stmt.table, TEST_TABLE, sizeof(stmt.table));
    stmt.has_where = 0;

    ResultSet *rs = db_select(&stmt, s);
    assert(rs != NULL);
    assert(rs->row_count == 0);
    result_free(rs);
    schema_free(s);
    PASS("db_select: empty/missing file -> 0 rows");
    return 0;
}

static int test_insert_then_select(void) {
    cleanup();
    TableSchema *s = make_test_schema();

    /* INSERT (1, alice) */
    InsertStmt ins = {0};
    strncpy(ins.table, TEST_TABLE, sizeof(ins.table));
    char *vals[] = {"1", "alice"};
    ins.values = vals;
    ins.value_count = 2;
    assert(db_insert(&ins, s) == SQL_OK);

    /* SELECT * */
    SelectStmt sel;
    sel.select_all = 1;
    sel.column_count = 0;
    sel.columns = NULL;
    strncpy(sel.table, TEST_TABLE, sizeof(sel.table));
    sel.has_where = 0;

    ResultSet *rs = db_select(&sel, s);
    assert(rs != NULL);
    assert(rs->row_count == 1);
    result_free(rs);
    schema_free(s);
    cleanup();
    PASS("db_insert then db_select -> 1 row");
    return 0;
}

int main(void) {
    int fail = 0;
    printf("=== test_executor ===\n");
    fail += test_insert_creates_file();
    fail += test_select_empty_file();
    fail += test_insert_then_select();
    printf("=== %s ===\n", fail == 0 ? "ALL PASS" : "FAILED");
    return fail;
}
