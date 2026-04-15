# 역할 A — B+ Tree 알고리즘

**담당자**: 김용  
**시작 가능**: Day 1 즉시 (외부 의존성 없음)

---

## 소유 파일 (이 파일만 수정 가능)

| 파일 | 설명 |
|------|------|
| `docs/role_A_김용.md` | 역할 A 명세 / 공유용 메모 |
| `include/bptree.h` | B+ Tree 공개 인터페이스 (헤더) |
| `src/bptree/bptree.c` | B+ Tree 구현체 |

## 절대 수정 금지 파일

```
include/interface.h       ← 전원 합의 없이 수정 금지
include/index_manager.h   ← role A 범위 밖, 무단 수정 금지
src/index/index_manager.c ← role A 범위 밖, 무단 수정 금지
src/input/                ← 박민석 소유
src/parser/               ← 김주형 소유
src/schema/               ← 김민철 소유
src/executor/             ← 김은재 소유
src/main.c                ← 공동 소유, 변경 전 팀 공지 필수
```

---

## 기능 명세

### 단일 키 B+ Tree

```c
BPTree *bptree_create(int order);
void    bptree_destroy(BPTree *tree);
int     bptree_insert(BPTree *tree, int key, long value);
long    bptree_search(BPTree *tree, int key);   // 없으면 -1
int     bptree_range(BPTree *tree, int from, int to,
                     long *out, int max_count); // BETWEEN 용
int     bptree_height(BPTree *tree);
void    bptree_print(BPTree *tree);             // 디버그 출력
```

- **같은 구현체를 두 번 사용**:
  - Tree #1: `id` 인덱스
  - Tree #2: `age` 인덱스
- **key**: 트리 인스턴스에 따라 `id` 또는 `age` (INT)
- **value**: `.dat` 파일에서 해당 행의 시작 오프셋 (`long`)
- 리프 노드는 양방향 링크드 리스트로 연결 (range query 지원)
- `age` 트리는 같은 나이가 여러 행에 나올 수 있으므로 **중복 key를 허용**해야 한다
- `id` 트리는 point search + range search 둘 다 사용된다
- `age` 트리는 주로 `WHERE age BETWEEN a AND b` 를 위한 range search에 사용된다
- 범위 조회 결과는 **key 오름차순**으로 반환한다
- 같은 key 가 여러 개면 **offset 오름차순**으로 반환해 결과 순서를 고정한다

---

## 구현 요구사항

### 알고리즘

- [ ] **노드 분열(split)**: 리프 / 내부 노드 모두 구현
- [ ] **루트 분열**: 분열 시 `height++`
- [ ] **리프 링크드 리스트**: `bptree_range()` 의 O(k) 순회를 위해 필수
- [ ] **중복 key 처리**: `age` 트리에서 같은 key에 여러 offset 이 저장될 수 있어야 함
- [ ] **반환 순서 고정**: range 결과는 key asc, same-key offsets asc

### `order` 파라미터 (트리 높이 제어)

| order | 의미 | 용도 |
|-------|------|------|
| `IDX_ORDER_DEFAULT` (128) | 낮은 트리, 빠른 탐색 | 운영 모드 |
| `IDX_ORDER_SMALL` (4) | 높은 트리, 느린 탐색 | 높이 비교 실험 |

### 디스크 I/O 시뮬레이션

`bptree_search` 내에서 레벨을 이동할 때마다 `IO_SLEEP()` 호출:

```c
// bptree.c 내부 — 이미 매크로 정의되어 있음
IO_SLEEP();  // BPTREE_SIMULATE_IO=1 일 때만 sleep 실행
```

빌드 방법:
```bash
make sim   # -DBPTREE_SIMULATE_IO=1 로 빌드
make       # 시뮬레이션 없이 빌드 (기본)
```

---

## 개발 순서

1. `BPNode` 내부 구조체 설계 (리프 / 내부 노드 구분)
2. `bptree_insert` → `bptree_search` → `bptree_range` 순서로 구현
3. `bptree_print` 로 구조 확인
4. 공용 `tests/test_bptree.c` 가 준비되면 해당 단위 테스트 통과 확인
5. `age` 트리 사용을 고려한 중복 key 동작 검증

---

## 단위 테스트 목표 (공용 하네스 준비 시)

현재 저장소에는 공용 `tests/test_bptree.c` 가 아직 없다.
따라서 현재 `make test` 는 역할 A B+ tree 회귀 검증 경로로 사용할 수 없다.
역할 A 구현은 먼저 로컬 검증 프로그램과 수동 재현으로 확인하고,
공용 하네스가 준비되면 아래 기준으로 다시 맞춘다.

| 테스트 | 기대 결과 |
|--------|----------|
| 1 ~ 1,000,000 순삽입 후 search(500000) | offset 반환 |
| 역순 삽입(1,000,000 → 1) 후 search | 정확히 일치 |
| range(100, 200) 후 반환 개수 | 101 |
| search(존재하지 않는 id) | -1 |
| 같은 age 여러 건 삽입 후 range(age, age) | 해당 offset 모두 반환 |
| order=4 vs order=128 트리 높이 비교 | order=4 가 더 높음 |

---

## 지켜야 할 점

1. `bptree.h` 에 선언된 API 시그니처를 변경하면 **반드시 팀 전체에 공지**한다.
2. 내부 구조체(`BPNode`)는 `.c` 파일 안에 정의하고 헤더에 노출하지 않는다.
3. 모든 `malloc/calloc` 에 NULL 체크를 한다.
4. `bptree_destroy` 에서 메모리 누수가 없어야 한다 (valgrind 통과 기준).

---

## 팀 의존성 / 회의 공유 메모

1. 현재 `WHERE id = ...` 인덱스 경로는 단일 offset 을 전제로 사용되고 있다.
2. 그런데 B+ tree 자체는 duplicate key 저장이 가능하므로, `id` 가 중복되면 `WHERE id = ...` 와 `WHERE id BETWEEN same AND same` 의 결과 의미가 달라질 수 있다.
3. 따라서 팀 전체 계약은 아래 둘 중 하나로 정리되어야 한다.
4. `id` 를 유일 키로 간주하고 상위 레이어에서 duplicate insert 를 막는다.
5. 또는 duplicate `id` 도 허용하고 `WHERE id = ...` 가 여러 행을 반환하도록 공용 계약과 caller 동작을 바꾼다.
6. 이 이슈는 역할 A 단독 수정 범위를 넘으므로, 현재는 문서로만 남기고 회의에서 공유 후 합의가 필요하다.
