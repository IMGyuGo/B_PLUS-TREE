# 테스트 명세 (TEST_SPEC.md)

각 담당자는 자신의 모듈 테스트를 tests/test_{module}.c 에 작성한다.
테스트 통과 기준: `make test` 전체 통과.

---

## 모듈1 — 입력 + 토크나이저 (박민석)

### input_read_file
- [ ] 존재하는 파일 → 내용 반환
- [ ] 존재하지 않는 파일 → NULL 반환

### lexer_tokenize
- [ ] `SELECT * FROM users` → 토큰 4개: SELECT, STAR, FROM, IDENT("users")
- [ ] `INSERT INTO users VALUES (1, 'alice')` → 첫 토큰이 INSERT
- [ ] `SELECT id, name FROM users` → 토큰 6개: SELECT, IDENT, COMMA, IDENT, FROM, IDENT
- [ ] `SELECT * FROM users WHERE id = 1` → WHERE, IDENT, EQ, INTEGER 포함
- [ ] 빈 문자열 → EOF 토큰 하나만
- [ ] 마지막 토큰은 항상 TOKEN_EOF

---

## 모듈2 — 파서 (김주형)

### parser_parse — SELECT
- [ ] `SELECT * FROM users` → type=STMT_SELECT, select_all=1, table="users"
- [ ] `SELECT id, name FROM users` → column_count=2, columns=["id","name"]
- [ ] `SELECT * FROM users WHERE id = 1` → has_where=1, where.col="id", where.val="1"

### parser_parse — INSERT
- [ ] `INSERT INTO users VALUES (1, 'alice', 30)` → type=STMT_INSERT, table="users", value_count=3
- [ ] values[0]="1", values[1]="alice", values[2]="30"

### 에러
- [ ] 빈 토큰 → NULL 반환
- [ ] 잘못된 문법 → NULL 반환 + stderr 출력

---

## 모듈3 — 스키마 (김민철)

### schema_load
- [ ] `schema/users.schema` 존재 → TableSchema 반환, table_name="users"
- [ ] column_count 가 파일 내용과 일치
- [ ] 존재하지 않는 테이블 → NULL 반환

### schema_validate
- [ ] INSERT 값 개수 == 스키마 컬럼 수 → SQL_OK
- [ ] INSERT 값 개수 != 스키마 컬럼 수 → SQL_ERR
- [ ] SELECT * → SQL_OK (컬럼 검사 불필요)
- [ ] SELECT 존재하는 컬럼 → SQL_OK
- [ ] SELECT 존재하지 않는 컬럼 → SQL_ERR
- [ ] WHERE 절 컬럼이 스키마에 없음 → SQL_ERR

---

## 모듈4 — 실행 + 저장소 (김은재)

### db_insert
- [ ] 정상 INSERT → `data/users.dat` 에 한 줄 추가, SQL_OK 반환
- [ ] 파일 없으면 자동 생성
- [ ] 연속 INSERT → 줄이 누적됨

### db_select
- [ ] SELECT * → 모든 컬럼, 모든 행 반환
- [ ] SELECT col → 해당 컬럼만 반환
- [ ] WHERE id = 1 → 조건에 맞는 행만 반환
- [ ] 파일 없음 → 빈 ResultSet (0 rows)

---

## E2E 테스트

### 시나리오 A: INSERT 후 SELECT
```bash
./sqlp samples/insert.sql   # "1 row inserted."
./sqlp samples/select.sql   # 데이터 출력
```
- [ ] insert.sql 실행 후 data/users.dat 파일 생성 확인
- [ ] select.sql 실행 후 stdout 에 삽입한 데이터 출력 확인

### 시나리오 B: 여러 번 INSERT
- [ ] 같은 insert.sql 3회 실행 → data/users.dat 에 3줄
- [ ] select.sql → 3행 출력
