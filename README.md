# 프로젝트 개요

JUNGLE CAMPUS 구성원의 인적사항을 SQL 문으로 파일에 저장하고 조회하는 프로그램

## 빌드 및 실행

```bash
make                        # 빌드 → ./sqlp 생성
./sqlp samples/insert.sql   # 실행
make test                   # 전체 단위 테스트
make clean                  # 빌드 산출물 삭제
```

---

## 처리 흐름(목차)

```
CLI Input → Lexer → Parser → Schema Load & Validate → Executor
```

---

## 주요 로직 소개

### 1. CLI Input

```bash
./sqlp samples/insert.sql
```

사용자는 터미널에서 실행 파일을 호출하고, 인자로 SQL 파일 경로를 전달.

`main` 함수에서 인자로 받은 경로를 `input_read_file` 함수에 넘기면, 파일을 열어 내용 전체를 하나의 문자열로 읽어 Lexer로 전달.
<br />
<br />

---

### 2. Lexer

Lexer는 input 단계에서 전달받은 문자열을 **의미 있는 토큰으로 쪼개는** 역할.

SQL 문자열을 앞에서부터 한 글자씩 읽으면서, 문자의 종류에 따라 토큰 유형을 판단.

| 입력 문자 | 처리 방식 | 생성되는 토큰 |
|-----------|-----------|---------------|
| 공백 / 줄바꿈 | 건너뜀 (줄바꿈은 줄 번호 +1) | — |
| 영문자 / `_` 로 시작하는 단어 | 키워드 목록과 대조 | `TOKEN_SELECT`, `TOKEN_INSERT` 등, 아니면 `TOKEN_IDENT` |
| 숫자 | 연속된 숫자를 모아서 처리 | `TOKEN_INTEGER` |
| `'` 작은따옴표로 감싼 값 | 닫는 `'` 까지 수집 | `TOKEN_STRING` |
| `*` `,` `(` `)` `=` `;` | 문자 하나 그대로 처리 | `TOKEN_STAR`, `TOKEN_LPAREN` 등 고유 토큰 타입 |
| 그 외 알 수 없는 문자 | 에러 출력 후 즉시 중단 | — |
<br />
<br />
<br />
<img width="2626" height="572" alt="image" src="https://github.com/user-attachments/assets/a91dde4f-2590-4bcf-84aa-771e10fcb25c" />
<br />
<br />
매칭된 토큰들은 `TokenList` 구조체에 배열로 저장. 배열 크기는 입력 크기에 따라 `realloc` 으로 동적으로 확장.

파일에 SQL 문장이 여러 개 있을 경우, `;` 기준으로 하나씩 잘라 Parser에 순서대로 전달.

---

### 3. Parser
```C
switch (peek(tokens, 0)->type) {
        case TOKEN_SELECT:
            return parse_select(tokens);
        case TOKEN_INSERT:
            return parse_insert(tokens);
```

Parser는 Lexer가 만든 토큰 배열을 받아 **문법을 검사하고 실행에 필요한 정보를 추출**하는 역할.

`peek` 함수로 0번째 토큰을 확인. SELECT이면 SELECT 파싱 로직으로, INSERT이면 INSERT 파싱 로직으로 분기.

분기 이후에는 토큰을 앞에서부터 순서대로 하나씩 소비하면서 해당 자리에 올 토큰이 맞는지 확인. 순서가 어긋나면 에러 출력 후 중단.
<br />
<br />


파싱에 성공하면 **ASTNode** 구조체를 반환. 실행에 필요한 정보만 구조화해서 담으며, 이후 Schema 검증과 Executor는 이 ASTNode만 보고 동작.

<img width="2312" height="1156" alt="image" src="https://github.com/user-attachments/assets/2b611385-45e7-4052-a6d8-8ba6eccfd62b" />


---

### 4. Schema Load & Validate

#### Schema Load

ASTNode에는 테이블 이름, 컬럼명, 값만 존재. **컬럼의 타입이나 개수** 같은 테이블 구조 정보는 SQL 문장 자체에 없기 때문에, 별도로 저장해둔 스키마 파일에서 로드.

`schema/{테이블명}.schema` 파일을 읽어 **TableSchema** 구조체로 변환. 실제 데이터는 건드리지 않고 테이블 구조 정보만 로드.

- 컬럼 개수
- 각 컬럼의 이름, 타입, 최대 길이

스키마 파일 형식:

```
table=users
columns=4
col0=student_no,INT,0
col1=name,VARCHAR,64
col2=gender,VARCHAR,10
col3=is_cs_major,BOOLEAN,0
```
<br />
<br />

#### Validate

Parser가 해석한 SQL 내용이 실제 테이블 구조와 맞는지 확인. 존재하지 않는 컬럼 조회, INT 컬럼에 문자열 삽입 등은 이 단계에서 차단.

**INSERT 검증**
- 값의 개수가 컬럼 수와 일치하는지 확인
- 각 값의 타입이 컬럼 타입과 맞는지 확인 (INT / VARCHAR 최대 길이 / BOOLEAN)

**SELECT 검증**
- `SELECT *` 는 무조건 통과
- 컬럼을 직접 지정했다면 해당 컬럼명이 테이블에 존재하는지 확인
- WHERE 절이 있다면 WHERE에 쓴 컬럼명이 테이블에 존재하는지 확인

검증 통과 시 ASTNode와 TableSchema가 함께 Executor로 전달. 실패 시 에러 메시지 출력 후 중단.

---

### 5. Executor

Executor는 ASTNode와 TableSchema를 받아 **실제로 파일에 데이터를 쓰거나 읽어오는** 마지막 단계.

**INSERT일 때**

ASTNode에 저장된 테이블명과 동일한 이름의 `.dat` 파일을 열어 값들을 한 줄로 저장. 컬럼을 지정한 경우 스키마 컬럼 순서에 맞게 재배열하여 저장. 지정하지 않은 컬럼은 빈 문자열로 채움.

**SELECT일 때**

`data/{테이블명}.dat` 파일을 한 줄씩 읽으면서 WHERE 조건에 맞는 행만 필터링. 걸러낸 행들로 ResultSet을 조립한 뒤 아래와 같이 출력.

```
+------------+-------+--------+-------------+
| student_no | name  | gender | is_cs_major |
+------------+-------+--------+-------------+
| 1          | alice | female | T           |
+------------+-------+--------+-------------+
(1 rows)
```

출력 후 ResultSet에 할당된 메모리를 전부 해제하고 종료.

---

## 빌드 상세

```bash
gcc -std=c99 -Wall -Wextra -Iinclude -o sqlp \
    src/main.c \
    src/input/input.c \
    src/input/lexer.c \
    src/parser/parser.c \
    src/schema/schema.c \
    src/executor/executor.c
```

6개의 .c 파일을 한 번에 컴파일하고 하나의 실행 파일로 링크.

---

## 쟁점 포인트

### 1. value 최대값 지정

VARCHAR 타입 컬럼에 대해 스키마 파일에 최대 길이를 미리 정의.
INSERT 실행 시 Validation 단계에서 입력값의 길이를 확인하여, 정의된 최대 길이를 초과하면 에러를 반환하고 실행을 중단.

- 스키마 파일의 각 컬럼 정의에 최대 길이(max_len)를 명시
- INSERT 시 Validation 단계에서 입력값 길이와 max_len 을 비교
- 초과 시 에러 메시지 출력 후 실행 중단

---

### 2. Column 지정하여 INSERT 기능 추가

INSERT 명령을 호출할 때, 삽입할 컬럼을 직접 지정 가능.

- `INSERT INTO users (name, student_no) VALUES ('김은재', 13)` 형태로 컬럼 지정 가능
- 지정한 컬럼 수와 값의 수가 다르면 Validation 단계에서 에러 반환
- 지정하지 않은 컬럼은 빈 문자열로 채워서 저장
- 컬럼 순서가 스키마 정의와 달라도 스키마 기준으로 재배열하여 저장

---

### 3. 복수의 SQL문 처리 기능 추가

하나의 SQL 파일 안에 여러 SQL 문장을 작성하고 한 번에 실행 가능.

- 하나의 파일에 INSERT, SELECT 등 여러 SQL 문장을 `;` 으로 구분하여 작성
- 전체 토큰을 `;` 기준으로 잘라 문장 하나씩 순서대로 실행
- 중간에 실패한 문장이 있어도 나머지 문장은 계속 실행

---

### 4. data/ 디렉토리 부재 처리

data/ 폴더는 실제 데이터 파일이 저장되는 디렉토리이지만 `.gitignore` 에 포함되어 있어, 저장소를 clone하면 해당 폴더가 존재하지 않는 문제.

- INSERT 실행 전 data/ 디렉토리 존재 여부를 확인
- 존재하지 않으면 자동으로 생성 후 실행을 이어나감

---
## 테스트 케이스
### :white_check_mark: Success Cases

| 구분 | 내용 |
|------|------|
| INSERT | 기본 INSERT 정상 동작 |
| INSERT | 일부 컬럼만 지정 |
| INSERT | 컬럼 순서 변경 |
| SELECT | 전체 조회 (`SELECT *`) |
| SELECT | 문자열 WHERE 조건 |

:point_right: 특징:
- Parser → Validation → Execution 전 단계 통과

---

### :x: Fail Cases

| 구분 | 내용 | 단계 |
|------|------|------|
| INSERT | 컬럼 수 부족 | Validation |
| INSERT | VARCHAR 길이 초과 | Validation |
| INSERT | 타입 불일치 | Validation |
| INSERT | 존재하지 않는 컬럼 | Validation |
| PARSE | 음수 리터럴 미지원 | Parser |
| PARSE | 세미콜론 미지원 | Parser |
| SELECT | 존재하지 않는 컬럼 조회 | Validation |
| SELECT | WHERE 컬럼 오류 | Validation |
| COMMON | 존재하지 않는 테이블 | Validation |
