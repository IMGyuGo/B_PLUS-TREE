#ifndef INTERFACE_H
#define INTERFACE_H

/* =========================================================
 * interface.h — 모듈 경계 계약
 *
 * 규칙:
 *   1. 선언(declaration)만 허용. 구현(definition) 금지.
 *   2. 수정 시 4명 전원 합의 필수.
 *   3. 메모리 소유권은 주석으로 명시한다.
 * ========================================================= */

/* =========================================================
 * 공통 에러 코드
 * ========================================================= */
#define SQL_OK   0
#define SQL_ERR -1

/* =========================================================
 * 모듈1 → 모듈2 : 토큰 목록
 * ========================================================= */
typedef enum {
    TOKEN_SELECT,
    TOKEN_INSERT,
    TOKEN_INTO,
    TOKEN_FROM,
    TOKEN_WHERE,
    TOKEN_VALUES,
    TOKEN_STAR,        /* * */
    TOKEN_COMMA,       /* , */
    TOKEN_LPAREN,      /* ( */
    TOKEN_RPAREN,      /* ) */
    TOKEN_EQ,          /* = */
    TOKEN_SEMICOLON,   /* ; */
    TOKEN_IDENT,       /* 테이블명, 컬럼명 */
    TOKEN_STRING,      /* 'alice' */
    TOKEN_INTEGER,     /* 42 */
    TOKEN_EOF
} TokenType;

typedef struct {
    TokenType type;
    char      value[256];  /* 토큰 원문 */
    int       line;        /* 에러 리포팅용 줄 번호 */
} Token;

typedef struct {
    Token *tokens;
    int    count;
} TokenList;

/* 모듈1 구현 (박민석) — 호출자가 free 책임 */
char      *input_read_file(const char *path);   /* 반환값: 호출자가 free() */
TokenList *lexer_tokenize(const char *sql);      /* 반환값: 호출자가 lexer_free() */
void       lexer_free(TokenList *list);

/* =========================================================
 * 모듈2 → 모듈3/4 : AST
 * ========================================================= */
typedef enum {
    STMT_SELECT,
    STMT_INSERT
} StmtType;

typedef struct {
    char col[64];   /* WHERE 컬럼명 */
    char val[256];  /* WHERE 비교값 */
} WhereClause;

typedef struct {
    int          select_all;    /* SELECT * 이면 1 */
    char       **columns;       /* SELECT col1, col2 → ["col1","col2"] */
    int          column_count;
    char         table[64];
    int          has_where;
    WhereClause  where;
} SelectStmt;

/* =========================================================
 * [기능 추가] INSERT 컬럼 지정 지원  (합의일: 2026-04-08, 제안: 김은재)
 *
 *   지원할 구문:
 *     INSERT INTO users (id, name) VALUES (1, 'alice')   ← 컬럼 지정
 *     INSERT INTO users VALUES (1, 'alice', 30)           ← 기존 (컬럼 미지정)
 *
 *   변경 내용:
 *     InsertStmt 에 columns / column_count 필드 추가.
 *     columns == NULL  &&  column_count == 0  이면 기존 방식(위치 기반)으로 동작.
 *
 *   ── 모듈별 수정 지시 ──────────────────────────────────────
 *
 *   [모듈2 — 파서 / 김주형]  src/parser/parser.c
 *     1. VALUES 앞에 '(' 가 오면 컬럼 목록으로 파싱한다.
 *        토큰 순서:  LPAREN  IDENT  (COMMA IDENT)*  RPAREN  TOKEN_VALUES ...
 *     2. 파싱된 컬럼명을 calloc 으로 복사해 node->insert.columns 에 저장.
 *     3. column_count 에 개수를 저장.
 *     4. 컬럼이 없으면 columns = NULL, column_count = 0 으로 유지.
 *     5. parser_free() 에서 columns 배열과 각 문자열을 free.
 *
 *   [모듈3 — 스키마 검증 / 김민철]  src/schema/schema.c
 *     1. column_count > 0 (컬럼 지정) 이면:
 *        a. columns[i] 가 스키마에 존재하는지 확인.
 *        b. column_count == value_count 인지 확인.
 *        c. 타입 검증은 columns[i] ↔ values[i] 를 매핑해서 수행.
 *     2. column_count == 0 (미지정) 이면 기존 로직 그대로 유지.
 *
 *   [모듈4 — 실행기 / 김은재]  src/executor/executor.c
 *     1. column_count > 0 이면 스키마 컬럼 순서 기준으로 값을 재배열 후 저장.
 *        예) 스키마: (id, name, age),  지정: (name, id),  값: ('alice', 1)
 *            → 저장 순서: 1,alice,  (age 는 "" 또는 DEFAULT 처리)
 *     2. column_count == 0 이면 기존 db_insert 로직 그대로 유지.
 * ========================================================= */
typedef struct {
    char   table[64];
    char **columns;      /* 지정된 컬럼명 목록. NULL = 미지정 (기존 방식) */
    int    column_count; /* 지정된 컬럼 수.   0   = 미지정 (기존 방식) */
    char **values;       /* VALUES 안의 값 목록 */
    int    value_count;
} InsertStmt;

typedef struct {
    StmtType type;
    union {
        SelectStmt select;
        InsertStmt insert;
    };
} ASTNode;

/* 모듈2 구현 (김주형) — 호출자가 free 책임 */
ASTNode *parser_parse(TokenList *tokens);  /* 반환값: 호출자가 parser_free() */
void     parser_free(ASTNode *node);

/* =========================================================
 * 모듈3 → 모듈4 : 스키마
 * ========================================================= */
typedef enum {
    COL_INT,
    COL_VARCHAR,
    COL_BOOLEAN  /* 허용값: "T" / "F" */
} ColType;

typedef struct {
    char    name[64];
    ColType type;
    int     max_len;    /* VARCHAR 전용, INT 는 0 */
} ColDef;

typedef struct {
    char    table_name[64];
    ColDef *columns;
    int     column_count;
} TableSchema;

/* 모듈3 구현 (김민철) — 호출자가 free 책임 */
TableSchema *schema_load(const char *table_name);              /* schema/{name}.schema 읽기 */
int          schema_validate(const ASTNode *node,
                             const TableSchema *schema);       /* SQL_OK or SQL_ERR */
void         schema_free(TableSchema *schema);

/* =========================================================
 * 모듈4 : 실행 결과
 * ========================================================= */
typedef struct {
    char **values;
    int    count;
} Row;

typedef struct {
    char **col_names;
    int    col_count;
    Row   *rows;
    int    row_count;
} ResultSet;

/* 모듈4 구현 (김은재) — 호출자가 free 책임 */
int        executor_run(const ASTNode *node, const TableSchema *schema);
ResultSet *db_select(const SelectStmt *stmt, const TableSchema *schema); /* 반환값: 호출자가 result_free() */
int        db_insert(const InsertStmt *stmt, const TableSchema *schema);
void       result_free(ResultSet *rs);

#endif /* INTERFACE_H */
