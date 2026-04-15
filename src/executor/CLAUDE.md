# 모듈4 — 실행 + 파일 저장소 (김은재)

## 담당 범위
- **이 디렉토리(src/executor/) 내 파일만 수정한다**
- **data/ 디렉토리의 데이터 파일 포맷을 설계하고 소유한다**
- 입력: ASTNode + TableSchema (검증 완료된 상태)
- 출력: 실행 결과 (INSERT → SQL_OK/ERR, SELECT → ResultSet)

## 구현 책임
| 함수 | 설명 |
|------|------|
| `executor_run(ast, schema)` | INSERT/SELECT 분기 실행 |
| `db_insert(stmt, schema)`   | 데이터를 `data/{table}.dat` 에 추가 |
| `db_select(stmt, schema)`   | `data/{table}.dat` 에서 데이터 조회 |
| `result_free(rs)`           | ResultSet 메모리 해제 |

## 데이터 파일 포맷 (설계 소유권: 김은재)
현재 형식: `data/{table_name}.dat` — CSV 권장
```
1,alice,30
2,bob,25
```
- 포맷 변경 시 이 CLAUDE.md 도 함께 업데이트
- 파일이 없으면 자동 생성

## db_select 동작
- SELECT * → 모든 컬럼 반환
- SELECT col1, col2 → 지정 컬럼만 반환
- WHERE col = val → 해당 조건에 맞는 행만 반환

## db_insert 동작
- `data/{table}.dat` 을 append 모드(`"a"`)로 열어 한 줄 추가
- 파일 없으면 새로 생성

## 에러 처리
- 파일 오픈 실패 → SQL_ERR + stderr 출력
- `exit()` 직접 호출 금지

## 메모리 규칙
- `db_select` 반환값: **호출자가 result_free()**
- result_free 에서 col_names, rows[i].values, rows, rs 전부 해제

## 테스트
- tests/test_executor.c 에 케이스 추가 가능
- `make test_executor` 로 단독 실행 가능해야 함
- 테스트용 data/ 파일은 테스트 시작 전 생성, 종료 후 정리
