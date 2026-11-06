CC := clang
CFLAGS ?= -std=gnu11 -O2 -Wall -Wextra -Iinclude
LDFLAGS ?= -lpthread

BUILD_DIR := build
BIN := $(BUILD_DIR)/handme
SRCS := src/main.c src/mongoose.c
OBJS := $(BUILD_DIR)/main.o $(BUILD_DIR)/mongoose.o

.PHONY: all clean run start format

all: $(BIN)

$(BIN): $(OBJS) | $(BUILD_DIR)
	$(CC) $(OBJS) $(LDFLAGS) -o $@

$(BUILD_DIR)/%.o: src/%.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR):
	mkdir -p $@

run: $(BIN)
	./$(BIN)

start: $(BIN)
	./start.sh

format:
	clang-format -i src/main.c

clean:
	rm -rf $(BUILD_DIR)
