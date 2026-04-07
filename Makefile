CC = gcc
CFLAGS = -Wall -Wextra -O2 -pthread
DEBUG_CFLAGS = -g -fsanitize=thread -pthread

SRCS = src/main.c src/bank.c src/transaction.c src/timer.c src/lock_mgr.c src/buffer_pool.c src/metrics.c
OBJS = $(SRCS:.c=.o)
TARGET = bankdb

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(OBJS) -o $(TARGET) $(CFLAGS)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

debug: CFLAGS = $(DEBUG_CFLAGS)
debug: $(TARGET)

clean:
	rm -f $(TARGET) $(OBJS)

test: $(TARGET)
	@echo "Running test cases..."
	./$(TARGET) --accounts=tests/accounts.txt --trace=tests/trace_simple.txt --deadlock=prevention
	./$(TARGET) --accounts=tests/accounts.txt --trace=tests/trace_readers.txt --deadlock=prevention
	./$(TARGET) --accounts=tests/accounts.txt --trace=tests/trace_deadlock.txt --deadlock=prevention
	./$(TARGET) --accounts=tests/accounts.txt --trace=tests/trace_abort.txt --deadlock=prevention
	./$(TARGET) --accounts=tests/accounts.txt --trace=tests/trace_buffer.txt --deadlock=prevention

.PHONY: all debug clean test