# 모듈1 — 입력 처리 + 토크나이저 (박민석)

## 담당 범위
- **이 디렉토리(src/input/) 내 파일만 수정한다**
- include/interface.h 의 Token, TokenList 구조체 사용 (수정 금지)

## 구현 책임
| 함수 | 설명 |
|------|------|
| `input_read_file(path)` | 파일 경로를 받아 SQL 문자열 전체를 읽어 반환 |
| `lexer_tokenize(sql)`   | SQL 문자열을 Token 배열로 분해 |
| `lexer_free(list)`      | TokenList 메모리 해제 |

## 지원해야 할 토큰
- 키워드: SELECT, INSERT, INTO, FROM, WHERE, VALUES
- 기호: `*` `,` `(` `)` `=`
- 값: 식별자(IDENT), 문자열 리터럴(`'alice'`), 정수 리터럴(`42`)
- 마지막은 반드시 TOKEN_EOF 로 끝낸다

## 에러 처리
- 파일 없음 → `input_read_file` 이 NULL 반환
- 알 수 없는 문자 → `fprintf(stderr, ...)` 후 NULL 반환
- `exit()` 직접 호출 금지

## 메모리 규칙
- `input_read_file` 반환값: **호출자가 free()**
- `lexer_tokenize` 반환값: **호출자가 lexer_free()**
- lexer_free 내에서 tokens 배열과 list 자체를 해제

## 테스트
- tests/test_input.c 에 케이스 추가 가능
- `make test_input` 으로 단독 실행 가능해야 함
