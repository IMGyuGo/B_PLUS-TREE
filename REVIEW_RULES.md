# REVIEW_RULES.md — AI 피드백 루프 규칙 누적

## 사용 방법
AI 코딩 → 테스트/리뷰 → 문제 발견 → 여기에 규칙 추가 → 다음 사이클부터 적용

형식:
```
- [날짜] [모듈] 규칙 내용
  이유: (왜 이 규칙이 생겼는가)
```

---

## 메모리 관리

*(발견 시 추가)*

---

## 에러 처리

*(발견 시 추가)*

---

## 인터페이스 계약

- [2026-04-08] [executor/main] main.c는 executor_run()을 통해서만 실행 단계를 호출해야 한다
  이유: main.c가 db_select/db_insert를 직접 호출하여 executor_run을 우회함.
  interface.h에 executor_run이 공개 API로 선언되어 있음에도 사용되지 않아 모듈 경계가 무너짐.
  수정 방향: executor_run의 INSERT 케이스에 "1 row inserted." 출력을 추가하고,
  main.c는 executor_run 한 줄로 대체한다.

- [2026-04-08] [executor/main] print_result는 executor.c 내부에만 존재해야 한다
  이유: executor_run 우회로 인해 main.c에 동일한 print_result static 함수가 중복 정의됨.
  executor_run을 올바르게 사용하면 자연히 해결된다.

---

## 파일 I/O

*(발견 시 추가)*

---

## 기타

*(발견 시 추가)*
