# SQL Parser Project — 마스터 지시서

## 프로젝트 개요
C 언어로 구현하는 파일 기반 SQL 처리기.
INSERT / SELECT 문을 파싱하고 파일에 데이터를 저장/조회한다.

## 아키텍처 흐름
```
입력 SQL
  → [모듈1: src/input/]   입력 읽기 + 토크나이저  (박민석)
  → TokenList
  → [모듈2: src/parser/]  파싱 + AST 생성         (김주형)
  → ASTNode
  → [모듈3: src/schema/]  스키마 로딩 + 검증       (김민철)
  → ASTNode + TableSchema
  → [모듈4: src/executor/] 실행 + 파일 저장소      (김은재)
  → 결과 출력
```

## 모듈 소유권 — 핵심 규칙

| 모듈 | 디렉토리 | 담당자 |
|------|----------|--------|
| 입력 + 토크나이저 | src/input/    | 박민석 |
| 파서             | src/parser/   | 김주형 |
| 스키마 + 검증    | src/schema/   | 김민철 |
| 실행 + 저장소    | src/executor/ | 김은재 |

- **자신의 디렉토리 외 파일은 절대 수정하지 않는다**
- **include/interface.h 는 4명 합의 후에만 수정** (수정 시 모든 팀원에게 공지)
- src/main.c 는 공동 소유 — 변경 전 팀 공지 필수

## 인터페이스 계약
- 모듈 간 데이터 구조 및 함수 시그니처는 **include/interface.h 에만** 정의
- 다른 모듈 함수는 반드시 interface.h 를 통해서만 호출
- 각 모듈의 내부 헬퍼 함수는 해당 .c 파일 내 static 으로 선언

## 코딩 규칙
- 언어: C99 (`gcc -std=c99`)
- 함수명: `snake_case`
- 에러 반환: `SQL_ERR` (-1), 성공: `SQL_OK` (0)
- `exit()` 직접 호출 금지 (main.c 제외)
- `malloc` 후 반드시 NULL 체크
- 메모리 소유권: interface.h 주석 기준 (호출자 free 원칙)
- 전역 변수 사용 금지
- 컴파일 경고 무시 금지 (`-Wall -Wextra` 클린 빌드 유지)

## 최초 세팅 (clone 후 1회 실행)
```bash
git config core.hooksPath .githooks   # pre-commit 훅 활성화
```

## 빌드 / 테스트
```bash
make               # 전체 빌드 → ./sqlp 생성
make test          # 전체 단위 테스트 실행
bash scripts/lint.sh  # 린트 단독 실행
./sqlp samples/insert.sql
./sqlp samples/select.sql
```

## 파일 레이아웃
```
include/interface.h      ← 모듈 경계 계약 (합의 없이 수정 금지)
src/main.c               ← 진입점 (공동)
src/input/               ← 박민석
src/parser/              ← 김주형
src/schema/              ← 김민철
src/executor/            ← 김은재
tests/                   ← 각 담당자가 자기 모듈 테스트 작성
schema/                  ← 테이블 스키마 정의 파일 (김민철 소유)
data/                    ← 테이블 데이터 파일 (김은재 소유)
samples/                 ← 테스트용 SQL 파일
```

## 참고 문서
- REVIEW_RULES.md  : AI 리뷰 중 발견된 규칙 누적 (발견 즉시 추가)
- tests/TEST_SPEC.md : 각 모듈 테스트 케이스 명세
