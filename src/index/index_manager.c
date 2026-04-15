/* =========================================================
 * index_manager.c — 인덱스 매니저 구현
 *
 * 담당자 : 김은재 (역할 B)
 * 금지   : 다른 팀원은 이 파일을 수정하지 않는다.
 *
 * 구현 체크리스트:
 *   [x] index_init: data/{table}.dat 스캔 -> 두 트리 동시 구축
 *       - ftell()로 각 행의 시작 오프셋 기록
 *       - 첫 번째 컬럼(id)  파싱 -> tree_id  에 삽입
 *       - 세 번째 컬럼(age) 파싱 -> tree_age 에 삽입
 *   [x] index_insert_id / index_search_id / index_range_id
 *   [x] index_insert_age / index_range_age
 *   [x] index_height_id / index_height_age
 *   [x] index_cleanup: 모든 트리 해제
 * ========================================================= */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../../include/interface.h"
#include "../../include/index_manager.h"
#include "../../include/bptree.h"

/* =========================================================
 * 내부 상태: 테이블별 인덱스 레코드
 * ========================================================= */
typedef struct {
    char    table[64];
    BPTree *tree_id;    /* Tree #1: id 단일 인덱스  */
    BPTree *tree_age;   /* Tree #2: age 단일 인덱스 */
    int     initialized;
} TableIndex;

static TableIndex g_tables[IDX_MAX_TABLES];
static int        g_count = 0;

/* =========================================================
 * 내부 헬퍼
 * ========================================================= */

/* 테이블 이름으로 인덱스 레코드를 찾는다 */
static TableIndex *find_entry(const char *table) {
    for (int i = 0; i < g_count; i++) {
        if (strcmp(g_tables[i].table, table) == 0)
            return &g_tables[i];
    }
    return NULL;
}

/*
 * col_value — '|' 구분자 기준 n번째 컬럼 값을 buf에 복사한다.
 *
 * 데이터 파일 형식: "1 | alice | 25 | alice@example.com\n"
 * n=0 -> "1", n=1 -> "alice", n=2 -> "25", ...
 *
 * 앞뒤 공백은 자동으로 제거한다.
 * 반환: 1 성공, 0 실패 (컬럼 없음)
 */
static int col_value(const char *line, int n, char *buf, int buf_size) {
    const char *p = line;
    int col = 0;

    /* n번째 컬럼 시작 위치까지 이동 */
    while (*p && col < n) {
        if (*p == '|') col++;
        p++;
    }
    if (col < n) return 0;

    /* 앞 공백 건너뜀 */
    while (*p == ' ') p++;

    /* '|', '\n', '\r', '\0' 전까지 복사 */
    int i = 0;
    while (*p && *p != '|' && *p != '\n' && *p != '\r' && i < buf_size - 1)
        buf[i++] = *p++;

    /* 뒤 공백 제거 */
    while (i > 0 && buf[i - 1] == ' ') i--;
    buf[i] = '\0';

    return 1;
}

/* =========================================================
 * index_init
 *
 * 두 트리를 생성하고, data/{table}.dat 가 존재하면
 * 전체를 스캔해 기존 데이터를 트리에 채운다.
 * 파일이 없으면 빈 인덱스로 시작한다 (정상).
 * ========================================================= */
int index_init(const char *table, int order_id, int order_age) {
    if (!table || g_count >= IDX_MAX_TABLES) return -1;

    /* 이미 초기화된 테이블이면 즉시 성공 반환 (멱등) */
    if (find_entry(table)) return 0;

    TableIndex *ti = &g_tables[g_count];
    memset(ti, 0, sizeof(TableIndex));
    strncpy(ti->table, table, sizeof(ti->table) - 1);

    int oid  = (order_id  > 2) ? order_id  : IDX_ORDER_DEFAULT;
    int oage = (order_age > 2) ? order_age : IDX_ORDER_DEFAULT;

    ti->tree_id  = bptree_create(oid);
    ti->tree_age = bptree_create(oage);
    if (!ti->tree_id || !ti->tree_age) {
        bptree_destroy(ti->tree_id);
        bptree_destroy(ti->tree_age);
        return -1;
    }

    g_count++;

    /* ── .dat 파일 스캔 → 두 트리 구축 ──────────────────── */
    char path[256];
    snprintf(path, sizeof(path), "data/%s.dat", table);

    FILE *fp = fopen(path, "rb");   /* binary mode: ftell 오프셋 정확도 */
    if (!fp) {
        /* 파일 없음 = 아직 INSERT 안 됨. 빈 인덱스로 시작. */
        fprintf(stderr, "[index] '%s' 초기화 완료 (파일 없음, 빈 인덱스)\n",
                table);
        ti->initialized = 1;
        return 0;
    }

    char line[1024];
    char col_buf[64];
    int  inserted = 0;

    while (1) {
        long offset = ftell(fp);            /* 이 행의 시작 오프셋 */
        if (!fgets(line, sizeof(line), fp)) break;

        /* 줄 끝 개행 제거 후 빈 줄 건너뜀 */
        int len = (int)strlen(line);
        while (len > 0 &&
               (line[len - 1] == '\n' || line[len - 1] == '\r'))
            line[--len] = '\0';
        if (len == 0) continue;

        /* id (컬럼 0) 파싱 */
        if (!col_value(line, 0, col_buf, sizeof(col_buf))) continue;
        int id = atoi(col_buf);

        /* age (컬럼 2) 파싱 */
        if (!col_value(line, 2, col_buf, sizeof(col_buf))) continue;
        int age = atoi(col_buf);

        bptree_insert(ti->tree_id,  id,  offset);
        bptree_insert(ti->tree_age, age, offset);
        inserted++;
    }

    fclose(fp);

    fprintf(stderr,
            "[index] '%s' 초기화 완료 — %d 행 로드 "
            "(order_id=%d, order_age=%d)\n",
            table, inserted, oid, oage);

    ti->initialized = 1;
    return 0;
}

/* =========================================================
 * index_cleanup
 * ========================================================= */
void index_cleanup(void) {
    for (int i = 0; i < g_count; i++) {
        bptree_destroy(g_tables[i].tree_id);
        bptree_destroy(g_tables[i].tree_age);
        g_tables[i].initialized = 0;
    }
    g_count = 0;
}

/* =========================================================
 * Tree #1 — id 단일 인덱스
 * ========================================================= */
int index_insert_id(const char *table, int id, long offset) {
    TableIndex *ti = find_entry(table);
    if (!ti) return -1;
    return bptree_insert(ti->tree_id, id, offset);
}

long index_search_id(const char *table, int id) {
    TableIndex *ti = find_entry(table);
    if (!ti) return -1;
    return bptree_search(ti->tree_id, id);
}

int index_range_id(const char *table, int from, int to,
                   long *offsets, int max) {
    TableIndex *ti = find_entry(table);
    if (!ti || !offsets) return 0;
    return bptree_range(ti->tree_id, from, to, offsets, max);
}

/* =========================================================
 * Tree #2 — age 단일 인덱스
 *   age 는 유일하지 않으므로 range search 만 제공한다.
 * ========================================================= */
int index_insert_age(const char *table, int age, long offset) {
    TableIndex *ti = find_entry(table);
    if (!ti) return -1;
    return bptree_insert(ti->tree_age, age, offset);
}

int index_range_age(const char *table, int from, int to,
                    long *offsets, int max) {
    TableIndex *ti = find_entry(table);
    if (!ti || !offsets) return 0;
    return bptree_range(ti->tree_age, from, to, offsets, max);
}

/* =========================================================
 * 높이 조회
 * ========================================================= */
int index_height_id(const char *table) {
    TableIndex *ti = find_entry(table);
    if (!ti) return 0;
    return bptree_height(ti->tree_id);
}

int index_height_age(const char *table) {
    TableIndex *ti = find_entry(table);
    if (!ti) return 0;
    return bptree_height(ti->tree_age);
}
