# 이전 SQL 처리기와 현재 B+ Tree 프로젝트의 연속성 정리

## 비교 기준

- 이전 프로젝트: `https://github.com/Developer-EJ/SQL-Parser`
- 현재 프로젝트: `btreeDB`

이 문서는 "이전 차수에서 만든 SQL 처리기와의 매끄러운 연결"이 실제로 어떤 의미였는지를 비교 관점에서 정리한 것이다.

## 핵심 결론

현재 프로젝트는 이전 SQL 처리기를 버리고 새로 만든 것이 아니다.

오히려 이전 프로젝트의 핵심 실행 파이프라인을 그대로 유지한 상태에서, `B+ Tree 인덱스`, `인덱스 매니저`, `성능 비교`, `BETWEEN 문법`, `대용량 데이터 생성`을 얹어서 확장한 형태에 가깝다.

즉, 이번 과제에서 말한 "매끄러운 연결"은 다음 뜻에 가깝다.

1. 기존 SQL 처리기의 입력, 파싱, 스키마 검증, 실행 흐름을 최대한 유지한다.
2. 인덱스는 SQL 처리기 바깥의 별도 프로그램이 아니라, 기존 `INSERT` / `SELECT` 실행 경로 안으로 들어간다.
3. 기존 선형 탐색 경로를 없애지 않고 남겨서, 인덱스 경로와 성능 비교가 가능하게 만든다.

## 이전 프로젝트에서 그대로 유지된 것

### 1. 실행 파이프라인

이전 프로젝트의 README 기준 처리 흐름은 아래와 같았다.

`CLI Input -> Lexer -> Parser -> Schema Load & Validate -> Executor`

현재 프로젝트도 이 흐름을 그대로 유지한다.

- `src/input/input.c`: SQL 파일 읽기
- `src/input/lexer.c`: 문자열을 토큰으로 변환
- `src/parser/parser.c`: 토큰을 AST로 변환
- `src/schema/schema.c`: 스키마 로딩과 의미 검증
- `src/executor/executor.c`: 실제 파일 읽기/쓰기 실행
- `src/main.c`: 전체 실행 루프 제어

즉, B+ Tree는 기존 파이프라인을 대체한 것이 아니라, 그 파이프라인의 뒤쪽 실행 단계에 추가된 것이다.

### 2. SQL 파일 실행 방식

이전 프로젝트는 대화형 DB가 아니라, SQL 파일을 넘겨 실행하는 구조였다.

예:

```bash
./sqlp samples/insert.sql
```

현재 프로젝트도 동일하게 SQL 파일 기반으로 실행한다.

즉, 사용 방식 자체를 바꾸지 않고 기존 인터페이스를 유지한 채 내부 실행 경로만 확장했다.

### 3. 데이터/스키마 기반 구조

이전 프로젝트는 다음 규칙을 썼다.

- `schema/{table}.schema`: 테이블 구조 정의
- `data/{table}.dat`: 실제 레코드 저장
- `samples/*.sql`: 입력 SQL 예시

현재 프로젝트도 이 구조를 유지한다.

즉, B+ Tree를 추가하면서도 저장 경로, 스키마 로딩 방식, 샘플 SQL 사용 방식은 그대로 이어받았다.

### 4. 선형 스캔 기반 SELECT 경로

이전 SQL 처리기의 `SELECT`는 `data/{table}.dat`를 처음부터 끝까지 읽는 선형 탐색 방식이었다.

현재 프로젝트도 이 경로를 제거하지 않았다.

오히려 이 선형 스캔을 그대로 남겨두었기 때문에:

- `WHERE id = ?` 또는 `WHERE id BETWEEN ...` 에서는 인덱스 경로
- `WHERE name = ?` 같은 경우에는 선형 스캔 경로

를 나눠서 비교할 수 있게 되었다.

이번 과제의 성능 비교 요구사항은 바로 이 "기존 경로를 살린 상태에서 인덱스 경로를 추가"하는 구조와 잘 맞는다.

## 현재 프로젝트에서 확장된 것

### 1. 모듈 확장

이전 프로젝트의 `src/`는 주로 아래 구성만 있었다.

- `src/input`
- `src/parser`
- `src/schema`
- `src/executor`
- `src/main.c`

현재 프로젝트에는 아래 모듈이 추가되었다.

- `include/bptree.h`
- `include/index_manager.h`
- `src/bptree/bptree.c`
- `src/index/index_manager.c`
- `tools/gen_data.c`

즉, 기존 SQL 처리기 위에 "인덱스 계층"과 "대용량 테스트 계층"이 추가된 구조다.

### 2. 인터페이스 계약 확장

이전 프로젝트의 `include/interface.h`는 기본적인 `SELECT`, `INSERT`, `WHERE col = val` 수준을 중심으로 설계되어 있었다.

현재 프로젝트에서는 여기에 최소한의 확장만 들어갔다.

- `TOKEN_BETWEEN`
- `TOKEN_AND`
- `WhereType`
- `WHERE_EQ`
- `WHERE_BETWEEN`
- `WhereClause.val_from`
- `WhereClause.val_to`

즉, 기존 AST와 토큰 계약을 완전히 바꾸지 않고, B+ Tree 범위 검색을 표현할 수 있는 만큼만 확장했다.

이게 "매끄러운 연결"의 중요한 포인트다.

파서 전체를 갈아엎지 않고, 기존 문법 모델 위에 `BETWEEN`만 얹어서 확장했기 때문이다.

### 3. Executor가 통합 지점이 됨

이전 프로젝트에서 Executor는 파일 기반 INSERT/SELECT를 수행하는 마지막 단계였다.

현재 프로젝트에서는 이 Executor가 기존 SQL 처리기와 인덱스를 연결하는 핵심 지점이 되었다.

#### INSERT 쪽 연결

이전:

- `.dat` 파일에 레코드를 한 줄 append

현재:

1. `.dat` 파일에 레코드를 append
2. 그 행의 시작 `offset`을 기록
3. `id`, `age` 값을 추출
4. `index_insert_id`, `index_insert_age`로 B+ Tree에 등록

즉, 기존 INSERT 결과를 그대로 인덱스 입력으로 재사용한다.

#### SELECT 쪽 연결

이전:

- WHERE 조건이 있어도 결국 파일을 선형 탐색

현재:

- `WHERE id = ?` -> `index_search_id`
- `WHERE id BETWEEN a AND b` -> `index_range_id`
- `WHERE age BETWEEN a AND b` -> `index_range_age`
- 그 외 -> 기존 선형 탐색 fallback

즉, 기존 SELECT를 폐기하지 않고, 가능한 조건에서만 인덱스를 태우는 방식이다.

이 덕분에 SQL 처리기와 인덱스가 충돌하지 않고 자연스럽게 연결된다.

### 4. main.c에서 lifecycle만 추가됨

현재 `main.c`를 보면 기존의:

1. SQL 파일 읽기
2. 토큰화
3. 세미콜론 기준 분리
4. statement별 파싱/검증/실행

흐름은 그대로다.

추가된 것은 주로:

- statement 실행 전에 `index_init(...)`
- 프로그램 종료 전에 `index_cleanup()`

정도다.

즉, 전체 실행 모델을 바꾸지 않고 인덱스 초기화/정리만 기존 루프에 끼워 넣었다.

## Role C 관점에서 연결이 매끄러운 이유

이번 과제에서 C 역할은 `lexer`, `parser`, `schema`를 맡고 있다.

이 부분이 매끄럽게 연결된 이유는, role C의 변경이 "실행기와 인덱스가 사용할 수 있을 정도로만 문법/검증을 확장"하는 수준으로 제한되었기 때문이다.

### lexer

이전 lexer는 기본 키워드와 식별자, 문자열, 숫자를 토큰화했다.

현재 lexer는 여기에:

- `BETWEEN`
- `AND`

두 키워드만 추가했다.

즉, 기존 토크나이징 구조를 그대로 유지한 채 범위 검색 문법을 위한 최소 확장만 넣었다.

### parser

이전 parser는 `WHERE col = val` 중심이었다.

현재 parser는 여기에:

- `WHERE col BETWEEN from AND to`

분기만 추가했다.

즉, AST 전체 구조를 바꾸기보다는 `WhereClause`에 타입과 양끝 값만 추가해서 기존 실행기로 넘길 수 있게 만들었다.

### schema

이전 schema 검증은:

- 컬럼 존재 여부
- 타입 일치 여부
- VARCHAR 길이 제한

중심이었다.

현재 schema는 여기에:

- `BETWEEN`은 INT 컬럼에서만 허용
- `BETWEEN`의 양쪽 값도 정수여야 함

검증만 추가했다.

즉, 인덱스 사용 전제 조건을 SQL 처리기 안에서 먼저 보장해 주는 역할로 자연스럽게 연결되었다.

## 스키마 자체도 과제 목적에 맞게 바뀜

이전 프로젝트의 `schema/users.schema`는 예를 들어 다음과 같은 컬럼을 가졌다.

- `student_no`
- `name`
- `gender`
- `is_cs_major`

현재 프로젝트의 `schema/users.schema`는 과제 목적에 맞게 아래처럼 바뀌었다.

- `id`
- `name`
- `age`
- `email`

이 변화는 단순한 예제 변경이 아니라, 이번 과제의 핵심 요구사항인:

- `ID 자동 부여 및 ID 인덱스`
- `ID 기준 SELECT`
- 다른 필드 기준 SELECT와의 비교
- 추가로 `age` 범위 조건 실험

을 수행하기 쉬운 구조로 바꾼 것이다.

즉, SQL 처리기의 형식은 유지하되 데이터 모델을 B+ Tree 실험에 맞게 재설계한 셈이다.

## 빌드와 테스트도 확장됨

이전 프로젝트 Makefile은 기본 실행 파일과 파서/스키마/실행기 테스트 중심이었다.

현재 프로젝트 Makefile은 여기에 다음이 추가되었다.

- `make sim`
- `make perf`
- `make perf_sim`
- `make gen_data`
- `test_bptree`
- `test_index`

즉, 이전 프로젝트가 "기능 구현과 검증" 중심이었다면, 현재 프로젝트는 여기에 "성능 비교"와 "대용량 실험"이 추가되었다.

## 발표에서 이렇게 설명하면 자연스럽다

발표에서는 다음 흐름으로 설명하면 좋다.

1. 이전 차수에서 SQL 파일을 읽고, Lexer/Parser/Schema/Executor를 거쳐 파일 DB를 실행하는 SQL 처리기를 이미 만들었다.
2. 이번 차수에서는 그 처리기를 버리지 않고 그대로 기반으로 사용했다.
3. Parser/Schema에는 `BETWEEN` 같은 최소 문법만 추가했다.
4. Executor에 인덱스 분기를 넣어 `WHERE id = ?`, `WHERE id BETWEEN ...`일 때만 B+ Tree를 사용하게 했다.
5. 그 외 조건은 기존 선형 탐색을 유지해서 성능 비교군으로 활용했다.
6. 그래서 이번 프로젝트의 핵심은 "새 DB를 다시 만든 것"이 아니라, "이전 SQL 처리기에 인덱스 계층을 자연스럽게 통합한 것"이라고 볼 수 있다.

## 현재 시점의 한계

문서 기준 현재 저장소에서는 다음 모듈이 아직 스텁 상태일 수 있다.

- `src/bptree/bptree.c`
- `src/index/index_manager.c`
- 일부 성능 테스트 코드

즉, 연결 설계와 파이프라인은 명확하지만, B+ Tree 알고리즘 자체와 인덱스 초기화의 완성도는 별도로 확인해야 한다.

그래도 프로젝트 구조 관점에서는 "이전 SQL 처리기와의 연결"이 이미 잘 설계되어 있다.

## 한 줄 요약

이전 프로젝트가 "SQL을 읽고 파일 DB를 실행하는 엔진"이었다면, 현재 프로젝트는 그 엔진을 그대로 유지한 채 `INSERT/SELECT` 실행 경로 안에 `B+ Tree 인덱스`와 `성능 비교 기능`을 끼워 넣은 확장판이다.
