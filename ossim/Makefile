# COMP2040 ossim. Builds with gcc or clang on Linux, macOS and WSL.
CC      ?= cc
CFLAGS  ?= -std=c99 -Wall -Wextra -Werror -O2 -g -Iinclude
SRC      = $(wildcard src/*.c)
OBJ      = $(SRC:src/%.c=build/%.o)
BIN      = ossim

all: $(BIN)

$(BIN): $(OBJ)
	$(CC) $(CFLAGS) -o $@ $^

build/%.o: src/%.c include/*.h | build
	$(CC) $(CFLAGS) -c -o $@ $<

build:
	mkdir -p build

test: $(BIN)
	sh tests/run.sh

clean:
	rm -rf build $(BIN)

.PHONY: all test clean
