CC ?= clang
CFLAGS ?= -std=gnu11 -O2 -Wall -Wextra -Iinclude
LDFLAGS ?=
LDLIBS ?=

BUILD_DIR := build
SRCS := src/main.c src/mongoose.c
OBJS := $(BUILD_DIR)/main.o $(BUILD_DIR)/mongoose.o

# Windows keeps a different binary suffix, needs winsock instead of pthreads,
# and mingw32-make defaults its recipe shell to sh.exe even when no sh is
# installed, so recipes there are always driven by cmd.exe.
ifeq ($(OS),Windows_NT)
  EXE := .exe
  LDLIBS += -lws2_32
  SHELL := cmd.exe
  .SHELLFLAGS := /c
  MKDIR_P = if not exist $(1) mkdir $(1)
  RM_RF = if exist $(1) rmdir /s /q $(1)
  RUN_BIN = $(subst /,\,$(BIN))
  START_RUN = powershell -NoProfile -ExecutionPolicy Bypass -File start.ps1
else
  EXE :=
  LDLIBS += -lpthread
  MKDIR_P = mkdir -p $(1)
  RM_RF = rm -rf $(1)
  RUN_BIN = ./$(BIN)
  START_RUN = ./start.sh
endif

BIN := $(BUILD_DIR)/handme$(EXE)

.PHONY: all clean run start format

all: $(BIN)

$(BIN): $(OBJS) | $(BUILD_DIR)
	$(CC) $(LDFLAGS) $(OBJS) $(LDLIBS) -o $@

$(BUILD_DIR)/%.o: src/%.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR):
	$(call MKDIR_P,$(BUILD_DIR))

run: $(BIN)
	$(RUN_BIN)

start: $(BIN)
	$(START_RUN)

format:
	clang-format -i src/main.c

clean:
	$(call RM_RF,$(BUILD_DIR))
