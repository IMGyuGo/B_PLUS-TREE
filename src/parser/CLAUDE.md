# 모듈2 — 파서 + AST 생성 (김주형)

## 담당 범위
- **이 디렉토리(src/parser/) 내 파일만 수정한다**
- 입력: `TokenList *` (모듈1 산출물)
- 출력: `ASTNode *` (모듈3/4에서 소비)

## 구현 책임
| 함수 | 설명 |
|------|------|
| `parser_parse(tokens)` | TokenList → ASTNode 변환 |
| `parser_free(node)`    | ASTNode 및 내부 동적 할당 메모리 해제 |

## 지원 문법
```
SELECT * FROM <table>
SELECT <col1>, <col2>, ... FROM <table>
SELECT * FROM <table> WHERE <col> = <val>

INSERT INTO <table> VALUES (<val1>, <val2>, ...)
```

## 구현 전략 (재귀하강 파서 권장)
1. 현재 토큰을 보고 SELECT / INSERT 분기
2. 각 문법 규칙을 함수 하나로 구현 (parse_select, parse_insert)
3. 토큰 소비 헬퍼: expect(tokens, pos, TOKEN_XXX)

## 에러 처리
- 예상 토큰이 없으면 `fprintf(stderr, "parse error at line %d\n", ...)` 후 NULL 반환
- `exit()` 직접 호출 금지

## 메모리 규칙
- `parser_parse` 반환값: **호출자가 parser_free()**
- ASTNode 내부의 `char **columns`, `char **values` 는 parser_free 에서 해제

## 테스트
- tests/test_parser.c 에 케이스 추가 가능
- `make test_parser` 로 단독 실행 가능해야 함
- lexer_tokenize 를 직접 호출해서 테스트해도 됨
