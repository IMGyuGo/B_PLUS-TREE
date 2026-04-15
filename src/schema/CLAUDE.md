# 모듈3 — 스키마 로딩 + 검증 (김민철)

## 담당 범위
- **이 디렉토리(src/schema/) 내 파일만 수정한다**
- **schema/ 디렉토리의 .schema 파일 포맷을 설계하고 소유한다**
- 입력: 테이블명 또는 ASTNode
- 출력: TableSchema (schema_load), 검증 결과 (schema_validate)

## 구현 책임
| 함수 | 설명 |
|------|------|
| `schema_load(table_name)` | `schema/{name}.schema` 파일을 읽어 TableSchema 반환 |
| `schema_validate(ast, schema)` | AST가 스키마에 맞는지 검증 |
| `schema_free(schema)` | TableSchema 메모리 해제 |

## 스키마 파일 포맷 (설계 소유권: 김민철)
현재 형식: `schema/users.schema`
```
table=users
columns=3
col0=id,INT,0
col1=name,VARCHAR,64
col2=age,INT,0
```
- 포맷 변경 시 이 CLAUDE.md 도 함께 업데이트

## schema_validate 검증 항목
- INSERT: VALUES 개수 == 컬럼 수
- INSERT: 각 값의 타입이 스키마와 일치 (INT → 숫자, VARCHAR → 문자열)
- SELECT: WHERE 절의 컬럼명이 스키마에 존재
- SELECT: 지정 컬럼명이 스키마에 존재 (SELECT * 는 항상 통과)

## 에러 처리
- 파일 없음 → NULL 반환 + stderr 출력
- 검증 실패 → SQL_ERR + stderr 에 구체적인 이유 출력
- `exit()` 직접 호출 금지

## 메모리 규칙
- `schema_load` 반환값: **호출자가 schema_free()**
- schema_free 에서 columns 배열과 schema 자체를 해제

## 테스트
- tests/test_schema.c 에 케이스 추가 가능
- `make test_schema` 로 단독 실행 가능해야 함
- schema/users.schema 파일이 존재한다고 가정하고 테스트 작성
