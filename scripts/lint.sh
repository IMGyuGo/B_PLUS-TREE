#!/bin/bash
# lint.sh — 린트 + 정적 분석 통합 스크립트
# 사용: bash scripts/lint.sh [파일경로 ...]
#   파일 미지정 시 src/ 전체 검사

set -e
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$ROOT"

FAIL=0

# ── 1. 컴파일 경고를 에러로 취급 (-Werror) ──────────────────
echo "[lint] 컴파일 경고 검사 (-Wall -Wextra -Werror)..."
gcc -std=c99 -Wall -Wextra -Werror -Iinclude \
    src/input/input.c \
    src/input/lexer.c \
    src/parser/parser.c \
    src/schema/schema.c \
    src/executor/executor.c \
    src/main.c \
    -o /dev/null 2>&1 \
  && echo "[OK] 컴파일 경고 없음" \
  || { echo "[FAIL] 컴파일 경고 발생"; FAIL=1; }

# ── 2. cppcheck 정적 분석 (설치된 경우에만) ──────────────────
if command -v cppcheck &>/dev/null; then
    echo "[lint] cppcheck 정적 분석..."
    cppcheck --enable=all --error-exitcode=1 \
             --suppress=missingIncludeSystem \
             -Iinclude src/ 2>&1 \
      && echo "[OK] cppcheck 통과" \
      || { echo "[FAIL] cppcheck 오류 발생"; FAIL=1; }
else
    echo "[lint] cppcheck 미설치 — 스킵 (brew install cppcheck 또는 choco install cppcheck)"
fi

# ── 3. clang-format 포맷 체크 (설치된 경우에만) ──────────────
if command -v clang-format &>/dev/null; then
    echo "[lint] clang-format 포맷 검사..."
    FORMAT_FAIL=0
    for f in $(find src -name "*.c" -o -name "*.h") include/interface.h; do
        diff <(clang-format "$f") "$f" > /dev/null 2>&1 \
          || { echo "[FAIL] 포맷 불일치: $f  →  clang-format -i $f 로 수정"; FORMAT_FAIL=1; }
    done
    [ $FORMAT_FAIL -eq 0 ] && echo "[OK] clang-format 통과" || FAIL=1
else
    echo "[lint] clang-format 미설치 — 스킵"
fi

# ── 결과 ─────────────────────────────────────────────────────
echo ""
if [ $FAIL -eq 0 ]; then
    echo "========== LINT PASS =========="
    exit 0
else
    echo "========== LINT FAIL =========="
    exit 1
fi
