# COMP2040 ossim. Builds with gcc or clang on Linux, macOS and WSL.
CC      ?= cc
CFLAGS  ?= -std=c99 -Wall -Wextra -Werror -O2 -g -Iinclude
SRC      = $(wildcard src/*.c)
OBJ      = $(SRC:src/%.c=build/%.o)
BIN      = ossim
# The workload generator is a separate program (tools/wlgen.c).
GEN      = wlgen

all: $(BIN) $(GEN)

$(BIN): $(OBJ)
	$(CC) $(CFLAGS) -o $@ $^

build/%.o: src/%.c include/*.h | build
	$(CC) $(CFLAGS) -c -o $@ $<

build:
	mkdir -p build

$(GEN): tools/wlgen.c
	$(CC) $(CFLAGS) -o $@ $<

test: $(BIN) $(GEN)
	sh tests/run.sh

clean:
	rm -rf build $(BIN) $(GEN)

.PHONY: all test clean
