#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "../../include/interface.h"

/* =========================================================
 * 모듈3: 스키마 로딩 + 검증 (김민철)
 *
 * 이 파일은 두 가지 일을 한다.
 *
 * 첫 번째: 스키마 파일 읽기
 *   schema 폴더 안에는 테이블 정보가 담긴 파일이 있다.
 *   예를 들어 schema/users.schema 파일 안에는
 *   "users 테이블에는 id, name, age 컬럼이 있고
 *    id는 숫자, name은 문자, age는 숫자다" 라는 정보가 있다.
 *   이 파일을 읽어서 C 프로그램이 쓸 수 있는 구조체로 만든다.
 *
 * 두 번째: SQL 검증
 *   사용자가 입력한 SQL이 테이블 정의에 맞는지 확인한다.
 *   예를 들어 컬럼이 3개인데 값을 2개만 넣으면 에러를 낸다.
 * ========================================================= */


/* =========================================================
 * 함수 이름: is_integer_string
 * 이 함수는 이 파일 안에서만 쓰인다. (static)
 *
 * 하는 일:
 *   문자열이 정수(숫자)인지 아닌지 확인한다.
 *   정수가 맞으면 1을 반환하고, 아니면 0을 반환한다.
 *
 * 예시:
 *   "42"   -> 1  (숫자만 있으니까 정수다)
 *   "-7"   -> 1  (음수도 정수다)
 *   "abc"  -> 0  (숫자가 아니다)
 *   ""     -> 0  (빈 문자열은 정수가 아니다)
 *   "-"    -> 0  (마이너스 기호만 있으면 정수가 아니다)
 *
 * 언제 쓰이나:
 *   INSERT 검증할 때, INT 타입 컬럼에 들어온 값이
 *   진짜 숫자인지 확인할 때 사용한다.
 * ========================================================= */
static int is_integer_string(const char *s) {
    /* s가 NULL이거나 빈 문자열이면 정수가 아니다 */
    if (!s || *s == '\0') return 0;

    int i = 0;

    /* 첫 글자가 '-' 이면 음수일 수 있으니 그 다음 글자부터 검사한다 */
    if (s[0] == '-') i = 1;

    /* '-' 뒤에 아무것도 없으면 정수가 아니다 */
    if (s[i] == '\0') return 0;

    /* 나머지 글자가 전부 숫자(0~9)인지 하나씩 확인한다 */
    for (; s[i] != '\0'; i++) {
        if (!isdigit((unsigned char)s[i])) return 0;
    }

    return 1;
}

/* =========================================================
 * 함수 이름: is_boolean_string
 * 이 함수는 이 파일 안에서만 쓰인다. (static)
 *
 * 하는 일:
 *   문자열이 BOOLEAN 값인지 아닌지 확인한다.
 *   "T" 또는 "F" 이면 1, 그 외에는 0을 반환한다.
 *
 * 언제 쓰이나:
 *   INSERT 검증할 때, BOOLEAN 타입 컬럼에 들어온 값이
 *   "T" 또는 "F" 인지 확인할 때 사용한다.
 * ========================================================= */
static int is_boolean_string(const char *s) {
    return (s && (strcmp(s, "T") == 0 || strcmp(s, "F") == 0));
}

/* =========================================================
 * 함수 이름: find_column
 * 이 함수는 이 파일 안에서만 쓰인다. (static)
 *
 * 하는 일:
 *   스키마(테이블 정의)에 특정 컬럼명이 있는지 찾는다.
 *   있으면 1, 없으면 0을 반환한다.
 *
 * 예시:
 *   users 테이블에 id, name, age 컬럼이 있을 때
 *   find_column(schema, "name") -> 1  (있다)
 *   find_column(schema, "email") -> 0  (없다)
 *
 * 언제 쓰이나:
 *   SELECT col1, col2 처럼 컬럼을 직접 지정했을 때,
 *   또는 WHERE col = val 처럼 WHERE 절에 컬럼이 있을 때,
 *   그 컬럼명이 실제로 테이블에 존재하는지 확인할 때 사용한다.
 * ========================================================= */
static int find_column(const TableSchema *schema, const char *col_name) {
    /* 스키마의 컬럼 목록을 처음부터 끝까지 하나씩 확인한다 */
    for (int i = 0; i < schema->column_count; i++) {
        /* 컬럼 이름이 일치하면 1을 반환한다 */
        if (strcmp(schema->columns[i].name, col_name) == 0) return 1;
    }
    /* 끝까지 찾았는데 없으면 0을 반환한다 */
    return 0;
}


/* =========================================================
 * 함수 이름: schema_load
 *
 * 하는 일:
 *   테이블 이름을 받아서, 그 테이블의 스키마 파일을 읽고
 *   TableSchema 구조체를 만들어서 반환한다.
 *
 * 입력:
 *   table_name: 테이블 이름 문자열 (예: "users")
 *
 * 반환값:
 *   성공하면: TableSchema 구조체 포인터를 반환한다.
 *             이 메모리는 호출한 쪽에서 schema_free()로 해제해야 한다.
 *   실패하면: NULL을 반환한다.
 *             실패 이유는 stderr에 출력된다.
 *
 * 파일 경로:
 *   table_name이 "users"이면 "schema/users.schema" 파일을 읽는다.
 *
 * 파일 형식:
 *   table=users        <- 테이블 이름
 *   columns=3          <- 컬럼이 몇 개인지
 *   col0=id,INT,0      <- 0번 컬럼: 이름=id, 타입=INT, 최대길이=0
 *   col1=name,VARCHAR,64  <- 1번 컬럼: 이름=name, 타입=VARCHAR, 최대길이=64
 *   col2=age,INT,0     <- 2번 컬럼: 이름=age, 타입=INT, 최대길이=0
 * ========================================================= */
TableSchema *schema_load(const char *table_name) {
    /* table_name이 NULL이면 바로 실패 처리한다 */
    if (!table_name) return NULL;

    /* 파일 경로를 만든다. 예: "schema/users.schema" */
    char path[256];
    snprintf(path, sizeof(path), "schema/%s.schema", table_name);

    /* 파일을 읽기 모드로 연다 */
    FILE *fp = fopen(path, "r");
    if (!fp) {
        /* 파일이 없거나 열 수 없으면 에러 메시지를 출력하고 NULL 반환 */
        fprintf(stderr, "schema: cannot open '%s'\n", path);
        return NULL;
    }

    /* TableSchema 구조체를 메모리에 할당한다. calloc은 0으로 초기화해준다 */
    TableSchema *s = calloc(1, sizeof(TableSchema));
    if (!s) { fclose(fp); return NULL; }

    char line[512];   /* 파일에서 읽은 한 줄을 저장할 공간 */
    int col_count = 0; /* 컬럼 개수를 임시로 저장할 변수 */

    /* 파일을 한 줄씩 읽는다 */
    while (fgets(line, sizeof(line), fp)) {

        /* 줄 끝에 있는 줄바꿈 문자(\n 또는 \r\n)를 제거한다 */
        line[strcspn(line, "\r\n")] = '\0';

        if (strncmp(line, "table=", 6) == 0) {
            /* "table=users" 라인이면 '=' 뒤의 "users"를 테이블 이름으로 저장한다 */
            strncpy(s->table_name, line + 6, sizeof(s->table_name) - 1);

        } else if (strncmp(line, "columns=", 8) == 0) {
            /* "columns=3" 라인이면 컬럼 개수를 읽고 ColDef 배열을 메모리에 할당한다 */
            col_count = atoi(line + 8); /* "3" -> 숫자 3으로 변환 */

            if (col_count <= 0) {
                /* 컬럼 개수가 0 이하이면 잘못된 파일이다 */
                fprintf(stderr, "schema: invalid column count in '%s'\n", path);
                free(s);
                fclose(fp);
                return NULL;
            }

            s->column_count = col_count;

            /* 컬럼 개수만큼 ColDef 배열을 메모리에 할당한다 */
            s->columns = calloc(col_count, sizeof(ColDef));
            if (!s->columns) {
                free(s);
                fclose(fp);
                return NULL;
            }

        } else if (strncmp(line, "col", 3) == 0) {
            /* "col0=id,INT,0" 형태의 라인을 파싱한다 */

            /* '=' 위치를 찾는다 */
            char *eq = strchr(line, '=');
            if (!eq) continue; /* '='이 없으면 이 줄은 건너뛴다 */

            /* "col" 바로 뒤의 숫자가 컬럼 인덱스다. "col0" -> 0, "col2" -> 2 */
            int idx = atoi(line + 3);

            /* 인덱스가 유효한 범위인지 확인한다 */
            if (idx < 0 || idx >= col_count || !s->columns) continue;

            /* '=' 뒤의 문자열을 파싱한다. 예: "id,INT,0" */
            char *val = eq + 1;
            char name[64]     = {0}; /* 컬럼 이름을 저장할 공간 */
            char type_str[32] = {0}; /* 컬럼 타입 문자열을 저장할 공간 */
            int  max_len      = 0;   /* 최대 길이를 저장할 변수 */

            /* 쉼표(,)를 기준으로 문자열을 세 부분으로 나눈다 */
            char *tok1 = strtok(val, ",");   /* 첫 번째: 컬럼 이름. 예: "id" */
            char *tok2 = strtok(NULL, ",");  /* 두 번째: 타입. 예: "INT" */
            char *tok3 = strtok(NULL, ",");  /* 세 번째: 최대 길이. 예: "0" */

            /* 세 부분 중 하나라도 없으면 이 줄은 건너뛴다 */
            if (!tok1 || !tok2 || !tok3) continue;

            strncpy(name,     tok1, sizeof(name) - 1);
            strncpy(type_str, tok2, sizeof(type_str) - 1);
            max_len = atoi(tok3); /* "64" -> 숫자 64로 변환 */

            /* 파싱한 정보를 컬럼 배열에 저장한다 */
            strncpy(s->columns[idx].name, name, sizeof(s->columns[idx].name) - 1);
            s->columns[idx].max_len = max_len;

            /* 타입 문자열을 ColType enum 값으로 변환한다 */
            if (strcmp(type_str, "INT") == 0) {
                s->columns[idx].type = COL_INT;       /* "INT" -> COL_INT */
            } else if (strcmp(type_str, "VARCHAR") == 0) {
                s->columns[idx].type = COL_VARCHAR;   /* "VARCHAR" -> COL_VARCHAR */
            } else if (strcmp(type_str, "BOOLEAN") == 0) {
                s->columns[idx].type = COL_BOOLEAN;   /* "BOOLEAN" -> COL_BOOLEAN */
            } else {
                /* INT도 VARCHAR도 아닌 알 수 없는 타입이면 에러 처리한다 */
                fprintf(stderr, "schema: unknown type '%s' for column '%s'\n",
                        type_str, name);
                schema_free(s);
                fclose(fp);
                return NULL;
            }
        }
    }

    fclose(fp); /* 파일 읽기가 끝났으니 파일을 닫는다 */

    /* columns= 라인이 없어서 컬럼 정보가 하나도 없으면 잘못된 파일이다 */
    if (s->column_count == 0 || !s->columns) {
        fprintf(stderr, "schema: missing column definitions in '%s'\n", path);
        schema_free(s);
        return NULL;
    }

    return s; /* 완성된 TableSchema 구조체를 반환한다 */
}


/* =========================================================
 * 함수 이름: schema_validate
 *
 * 하는 일:
 *   파서(모듈2)가 만든 AST(SQL 구문 정보)가
 *   스키마(테이블 정의)에 맞는지 검사한다.
 *   맞으면 SQL_OK(0)를 반환하고, 틀리면 SQL_ERR(-1)을 반환한다.
 *
 * INSERT일 때 검사하는 것:
 *   1. VALUES에 넣은 값의 개수가 테이블 컬럼 수와 같은지 확인한다.
 *      예: 컬럼이 3개인데 VALUES (1, 'alice')처럼 2개만 넣으면 에러
 *   2. 각 값의 타입이 컬럼 타입과 맞는지 확인한다.
 *      예: INT 타입 컬럼에 'alice' 같은 문자를 넣으면 에러
 *
 * SELECT일 때 검사하는 것:
 *   1. SELECT * 이면 모든 컬럼을 가져오는 것이므로 무조건 통과한다.
 *   2. SELECT id, name 처럼 컬럼을 직접 지정했으면
 *      그 컬럼명이 테이블에 실제로 있는지 확인한다.
 *   3. WHERE 절이 있으면 WHERE에 쓴 컬럼명도
 *      테이블에 실제로 있는지 확인한다.
 * ========================================================= */
int schema_validate(const ASTNode *node, const TableSchema *schema) {
    /* node나 schema가 NULL이면 검증할 수 없으니 에러를 반환한다 */
    if (!node || !schema) return SQL_ERR;

    if (node->type == STMT_INSERT) {
        const InsertStmt *ins = &node->insert;

        if (ins->column_count > 0) {
            /* ── 컬럼 지정 방식: INSERT INTO users (id, name) VALUES (1, 'alice') ── */

            /* 지정한 컬럼 수와 값의 수가 다르면 에러다 */
            if (ins->column_count != ins->value_count) {
                fprintf(stderr, "schema: INSERT column count %d != value count %d\n",
                        ins->column_count, ins->value_count);
                return SQL_ERR;
            }

            /* 각 컬럼명이 스키마에 존재하는지, 타입이 맞는지 하나씩 확인한다 */
            for (int i = 0; i < ins->column_count; i++) {
                const char *col_name = ins->columns[i]; /* 지정한 컬럼명 */
                const char *val      = ins->values[i];  /* 해당 컬럼에 넣을 값 */

                /* 컬럼명이 스키마에 있는지 찾는다 */
                int found = -1;
                for (int j = 0; j < schema->column_count; j++) {
                    if (strcmp(schema->columns[j].name, col_name) == 0) {
                        found = j;
                        break;
                    }
                }
                if (found == -1) {
                    /* 스키마에 없는 컬럼명이면 에러다 */
                    fprintf(stderr, "schema: unknown column '%s' in INSERT\n", col_name);
                    return SQL_ERR;
                }

                /* 찾은 컬럼의 타입과 값이 맞는지 확인한다 */
                ColType expected = schema->columns[found].type;

                if (expected == COL_INT && !is_integer_string(val)) {
                    fprintf(stderr, "schema: column '%s' expects INT, got '%s'\n",
                            col_name, val);
                    return SQL_ERR;
                }

                if (expected == COL_VARCHAR) {
                    int max_len = schema->columns[found].max_len;
                    if (max_len > 0 && (int)strlen(val) > max_len) {
                        fprintf(stderr,
                                "schema: column '%s' max length is %d, got %d\n",
                                col_name, max_len, (int)strlen(val));
                        return SQL_ERR;
                    }
                }

                if (expected == COL_BOOLEAN && !is_boolean_string(val)) {
                    fprintf(stderr, "schema: column '%s' expects BOOLEAN (T/F), got '%s'\n",
                            col_name, val);
                    return SQL_ERR;
                }
            }

        } else {
            /* ── 컬럼 미지정 방식 (기존): INSERT INTO users VALUES (1, 'alice', 30) ── */

            /* VALUES 개수와 컬럼 수가 다르면 에러다 */
            if (ins->value_count != schema->column_count) {
                fprintf(stderr, "schema: INSERT expects %d values, got %d\n",
                        schema->column_count, ins->value_count);
                return SQL_ERR;
            }

            /* 각 값의 타입을 하나씩 검사한다 */
            for (int i = 0; i < ins->value_count; i++) {
                const char *val      = ins->values[i];
                ColType     expected = schema->columns[i].type;

                /* INT 타입 컬럼에 숫자가 아닌 값이 들어오면 에러다 */
                if (expected == COL_INT && !is_integer_string(val)) {
                    fprintf(stderr, "schema: column '%s' expects INT, got '%s'\n",
                            schema->columns[i].name, val);
                    return SQL_ERR;
                }

                /* VARCHAR 타입 컬럼은 값의 길이가 max_len을 초과하면 에러다.
                 * max_len이 0이면 길이 제한이 없는 것으로 간주한다. */
                if (expected == COL_VARCHAR) {
                    int max_len = schema->columns[i].max_len;
                    if (max_len > 0 && (int)strlen(val) > max_len) {
                        fprintf(stderr,
                                "schema: column '%s' max length is %d, got %d\n",
                                schema->columns[i].name, max_len, (int)strlen(val));
                        return SQL_ERR;
                    }
                }

                if (expected == COL_BOOLEAN && !is_boolean_string(val)) {
                    fprintf(stderr, "schema: column '%s' expects BOOLEAN (T/F), got '%s'\n",
                            schema->columns[i].name, val);
                    return SQL_ERR;
                }
            }
        }

        return SQL_OK; /* 모든 검사를 통과하면 성공 */

    } else if (node->type == STMT_SELECT) {
        const SelectStmt *sel = &node->select;

        /* SELECT * 은 모든 컬럼을 가져오는 것이므로 바로 통과한다 */
        if (sel->select_all) return SQL_OK;

        /* SELECT col1, col2 처럼 컬럼을 직접 지정한 경우,
         * 각 컬럼명이 테이블에 있는지 하나씩 확인한다 */
        for (int i = 0; i < sel->column_count; i++) {
            if (!find_column(schema, sel->columns[i])) {
                /* 해당 컬럼명이 테이블에 없으면 에러다 */
                fprintf(stderr, "schema: unknown column '%s' in SELECT\n",
                        sel->columns[i]);
                return SQL_ERR;
            }
        }

        /* WHERE 절이 있는 경우, WHERE에 쓴 컬럼명이 테이블에 있는지 확인한다 */
        if (sel->has_where) {
            if (!find_column(schema, sel->where.col)) {
                /* WHERE에 쓴 컬럼명이 테이블에 없으면 에러다 */
                fprintf(stderr, "schema: unknown column '%s' in WHERE\n",
                        sel->where.col);
                return SQL_ERR;
            }
        }

        return SQL_OK; /* 모든 검사를 통과하면 성공 */
    }

    /* INSERT도 SELECT도 아닌 알 수 없는 SQL 타입이면 에러를 반환한다 */
    fprintf(stderr, "schema: unknown statement type\n");
    return SQL_ERR;
}


/* =========================================================
 * 함수 이름: schema_free
 *
 * 하는 일:
 *   schema_load()가 메모리에 할당한 TableSchema 구조체를
 *   모두 해제한다. 프로그램이 메모리를 낭비하지 않도록
 *   사용이 끝난 스키마는 반드시 이 함수로 해제해야 한다.
 *
 * 해제 순서:
 *   1. columns 배열을 해제한다. (ColDef 배열)
 *   2. TableSchema 구조체 자체를 해제한다.
 *
 * 참고:
 *   schema가 NULL이어도 에러 없이 안전하게 처리한다.
 * ========================================================= */
void schema_free(TableSchema *schema) {
    if (!schema) return;
    free(schema->columns); /* 컬럼 배열 메모리 해제 */
    free(schema);           /* TableSchema 구조체 메모리 해제 */
}
