#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#ifdef _WIN32
#include <direct.h>
#define MKDIR(p) mkdir(p)
#else
#define MKDIR(p) mkdir(p, 0755)
#endif
#include "../../include/interface.h"

/* ──────────────────────────────────────────────
 * 내부 헬퍼 (static)
 * ────────────────────────────────────────────── */

/* 파일이 없을 때 빈 ResultSet 반환 */
static ResultSet *make_empty_rs(const TableSchema *schema) {
    ResultSet *rs = calloc(1, sizeof(ResultSet));
    if (!rs) return NULL;
    rs->col_count = schema->column_count;
    rs->col_names = calloc(rs->col_count, sizeof(char *));
    if (!rs->col_names) { free(rs); return NULL; }
    for (int i = 0; i < rs->col_count; i++)
        rs->col_names[i] = strdup(schema->columns[i].name);
    rs->row_count = 0;
    rs->rows = NULL;
    return rs;
}

/* 한 줄이 WHERE 조건에 맞으면 1, 아니면 0 */
static int line_matches_where(const char *line, const SelectStmt *stmt,
                              const TableSchema *schema) {
    if (!stmt->has_where) return 1;

    int col_idx = -1;
    for (int i = 0; i < schema->column_count; i++) {
        if (strcmp(schema->columns[i].name, stmt->where.col) == 0) {
            col_idx = i;
            break;
        }
    }
    if (col_idx < 0) return 1;

    char tmp[1024];
    strncpy(tmp, line, sizeof(tmp));
    char *tok = strtok(tmp, "|");
    for (int i = 0; i < col_idx && tok; i++)
        tok = strtok(NULL, "|");
    /* 앞뒤 공백 제거 */
    if (tok) {
        while (*tok == ' ') tok++;
        char *end = tok + strlen(tok) - 1;
        while (end > tok && *end == ' ') *end-- = '\0';
    }
    return (tok && strcmp(tok, stmt->where.val) == 0) ? 1 : 0;
}

/* CSV 한 줄 → Row 파싱 */
static Row parse_line_to_row(const char *line, const TableSchema *schema) {
    Row row = {0};
    row.count = schema->column_count;
    row.values = calloc(row.count, sizeof(char *));
    if (!row.values) return row;

    char buf[1024];
    strncpy(buf, line, sizeof(buf));
    char *tok = strtok(buf, "|");
    for (int i = 0; i < row.count; i++) {
        /* 앞뒤 공백 제거 */
        if (tok) {
            while (*tok == ' ') tok++;
            char *end = tok + strlen(tok) - 1;
            while (end > tok && *end == ' ') *end-- = '\0';
        }
        row.values[i] = strdup(tok ? tok : "");
        tok = strtok(NULL, "|");
    }
    return row;
}

/* 파일 전체를 읽어 WHERE 필터링 후 rows_out 에 저장.
 * 반환값: 읽은 행 수 (calloc 실패 시 -1) */
static int read_rows(FILE *fp, const SelectStmt *stmt,
                     const TableSchema *schema, Row **rows_out) {
    *rows_out = NULL;
    int capacity = 16;
    Row *rows = calloc(capacity, sizeof(Row));
    if (!rows) return -1;
    int row_count = 0;

    char line[1024];
    while (fgets(line, sizeof(line), fp)) {
        int len = (int)strlen(line);
        while (len > 0 && (line[len-1] == '\n' || line[len-1] == '\r'))
            line[--len] = '\0';
        if (len == 0) continue;

        if (!line_matches_where(line, stmt, schema)) continue;

        if (row_count == capacity) {
            capacity *= 2;
            Row *tmp = realloc(rows, capacity * sizeof(Row));
            if (!tmp) break;
            rows = tmp;
        }

        rows[row_count] = parse_line_to_row(line, schema);
        if (!rows[row_count].values) break;
        row_count++;
    }

    *rows_out = rows;
    return row_count;
}

/* rows → ResultSet 조립 (SELECT * / 지정 컬럼 분기 포함).
 * 실패 시 rows 메모리까지 해제하고 NULL 반환 */
static ResultSet *build_resultset(Row *rows, int row_count,
                                  const SelectStmt *stmt,
                                  const TableSchema *schema) {
    ResultSet *rs = calloc(1, sizeof(ResultSet));
    if (!rs) {
        for (int i = 0; i < row_count; i++) {
            for (int j = 0; j < rows[i].count; j++) free(rows[i].values[j]);
            free(rows[i].values);
        }
        free(rows);
        return NULL;
    }

    if (stmt->select_all) {
        rs->col_count = schema->column_count;
        rs->col_names = calloc(rs->col_count, sizeof(char *));
        for (int i = 0; i < rs->col_count; i++)
            rs->col_names[i] = strdup(schema->columns[i].name);
        rs->rows = rows;
        rs->row_count = row_count;
    } else {
        /* 지정 컬럼만 추출 — 스키마 인덱스 매핑 */
        rs->col_count = stmt->column_count;
        rs->col_names = calloc(rs->col_count, sizeof(char *));
        int *idx = calloc(rs->col_count, sizeof(int));

        for (int c = 0; c < stmt->column_count; c++) {
            rs->col_names[c] = strdup(stmt->columns[c]);
            idx[c] = -1;
            for (int s = 0; s < schema->column_count; s++) {
                if (strcmp(schema->columns[s].name, stmt->columns[c]) == 0) {
                    idx[c] = s;
                    break;
                }
            }
        }

        rs->rows = calloc(row_count, sizeof(Row));
        rs->row_count = row_count;
        for (int r = 0; r < row_count; r++) {
            rs->rows[r].count = rs->col_count;
            rs->rows[r].values = calloc(rs->col_count, sizeof(char *));
            for (int c = 0; c < rs->col_count; c++) {
                int si = idx[c];
                rs->rows[r].values[c] = strdup(
                    (si >= 0 && si < rows[r].count) ? rows[r].values[si] : "");
            }
            for (int j = 0; j < rows[r].count; j++) free(rows[r].values[j]);
            free(rows[r].values);
        }
        free(rows);
        free(idx);
    }

    return rs;
}

/* ──────────────────────────────────────────────
 * SELECT 결과 터미널 출력
 * ────────────────────────────────────────────── */
static void print_result(const ResultSet *rs) {
    if (!rs || rs->row_count == 0) {
        printf("(0 rows)\n");
        return;
    }
    for (int c = 0; c < rs->col_count; c++) {
        printf("%s", rs->col_names[c]);
        if (c < rs->col_count - 1) printf(" | ");
    }
    printf("\n");
    for (int r = 0; r < rs->row_count; r++) {
        for (int c = 0; c < rs->rows[r].count; c++) {
            printf("%s", rs->rows[r].values[c]);
            if (c < rs->rows[r].count - 1) printf(" | ");
        }
        printf("\n");
    }
    printf("(%d row%s)\n", rs->row_count, rs->row_count == 1 ? "" : "s");
}

/* ──────────────────────────────────────────────
 * 공개 API
 * ────────────────────────────────────────────── */

int executor_run(const ASTNode *node, const TableSchema *schema) {
    if (!node || !schema) return SQL_ERR;

    switch (node->type) {
        case STMT_INSERT:
            if (db_insert(&node->insert, schema) != SQL_OK) return SQL_ERR;
            printf("1 row inserted.\n");
            return SQL_OK;
        case STMT_SELECT: {
            ResultSet *rs = db_select(&node->select, schema);
            if (!rs) return SQL_ERR;
            print_result(rs);
            result_free(rs);
            return SQL_OK;
        }
        default:
            fprintf(stderr, "executor: unknown statement type\n");
            return SQL_ERR;
    }
}

int db_insert(const InsertStmt *stmt, const TableSchema *schema) {
    /* data/ 디렉토리가 없으면 자동으로 만든다.
     * 이미 존재하면 mkdir이 실패하지만 무시해도 된다. */
    MKDIR("data");

    char path[256];
    snprintf(path, sizeof(path), "data/%s.dat", stmt->table);

    FILE *fp = fopen(path, "a");
    if (!fp) {
        fprintf(stderr, "executor: cannot open '%s'\n", path);
        return SQL_ERR;
    }

    if (stmt->column_count == 0) {
        /* 컬럼 미지정: 기존 위치 기반 저장 */
        for (int i = 0; i < stmt->value_count; i++) {
            fprintf(fp, "%s", stmt->values[i]);
            if (i < stmt->value_count - 1) fprintf(fp, " | ");
        }
    } else {
        /* 컬럼 지정: 스키마 컬럼 순서 기준으로 값 재배열 후 저장
         * 지정되지 않은 컬럼은 빈 문자열로 채운다 */
        for (int s = 0; s < schema->column_count; s++) {
            int val_idx = -1;
            for (int c = 0; c < stmt->column_count; c++) {
                if (strcmp(stmt->columns[c], schema->columns[s].name) == 0) {
                    val_idx = c;
                    break;
                }
            }
            fprintf(fp, "%s", val_idx >= 0 ? stmt->values[val_idx] : "");
            if (s < schema->column_count - 1) fprintf(fp, " | ");
        }
    }
    fprintf(fp, "\n");
    fclose(fp);
    return SQL_OK;
}

ResultSet *db_select(const SelectStmt *stmt, const TableSchema *schema) {
    char path[256];
    snprintf(path, sizeof(path), "data/%s.dat", stmt->table);

    FILE *fp = fopen(path, "r");
    if (!fp) return make_empty_rs(schema);

    Row *rows = NULL;
    int row_count = read_rows(fp, stmt, schema, &rows);
    fclose(fp);
    if (row_count < 0) return NULL;

    return build_resultset(rows, row_count, stmt, schema);
}

void result_free(ResultSet *rs) {
    if (!rs) return;

    for (int i = 0; i < rs->row_count; i++) {
        for (int j = 0; j < rs->rows[i].count; j++)
            free(rs->rows[i].values[j]);
        free(rs->rows[i].values);
    }
    free(rs->rows);

    for (int i = 0; i < rs->col_count; i++)
        free(rs->col_names[i]);
    free(rs->col_names);

    free(rs);
}
