CC     = gcc
CFLAGS = -std=c99 -Wall -Wextra -Iinclude

SRCS   = src/main.c \
         src/input/input.c \
         src/input/lexer.c \
         src/parser/parser.c \
         src/schema/schema.c \
         src/executor/executor.c

TARGET = sqlp

# ── 기본 빌드 ──────────────────────────────────────────────
all: $(TARGET)

$(TARGET): $(SRCS)
	$(CC) $(CFLAGS) -o $@ $^

# ── 단위 테스트 ────────────────────────────────────────────
TEST_BINS = test_input test_parser test_schema test_executor

test: $(TEST_BINS)
	@echo ""
	@echo "========== Running Tests =========="
	@./test_input    && echo "[PASS] input"    || echo "[FAIL] input"
	@./test_parser   && echo "[PASS] parser"   || echo "[FAIL] parser"
	@./test_schema   && echo "[PASS] schema"   || echo "[FAIL] schema"
	@./test_executor && echo "[PASS] executor" || echo "[FAIL] executor"
	@echo "==================================="

test_input: tests/test_input.c src/input/input.c src/input/lexer.c
	$(CC) $(CFLAGS) -o $@ $^

test_parser: tests/test_parser.c \
             src/input/input.c src/input/lexer.c \
             src/parser/parser.c
	$(CC) $(CFLAGS) -o $@ $^

test_schema: tests/test_schema.c \
             src/schema/schema.c
	$(CC) $(CFLAGS) -o $@ $^

test_executor: tests/test_executor.c \
               src/schema/schema.c \
               src/executor/executor.c
	$(CC) $(CFLAGS) -o $@ $^

# ── 정리 ───────────────────────────────────────────────────
clean:
	rm -f $(TARGET) $(TEST_BINS) src/**/*.o

.PHONY: all test clean
