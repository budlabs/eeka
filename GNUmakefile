.PHONY: all install-dev uninstall-dev run clean dirs

.ONESHELL:
.DEFAULT_GOAL       := all

SHELL               := /bin/bash

NAME := eeka
BUILD_DIR ?= build

# Source files
SRC := $(wildcard *.c)
# Object files (in build directory)
OBJ := $(patsubst %.c,$(BUILD_DIR)/%.o,$(SRC))

# Compiler flags
CPPFLAGS += -Wall -Wextra -std=c99 -D_GNU_SOURCE -O2
# Linker flags
LDFLAGS += -lxcb -lxcb-keysyms -lxcb-xtest

all: dirs $(BUILD_DIR)/$(NAME)

# Create build directory
dirs:
	mkdir -p $(BUILD_DIR)

# Compile step: .c to .o
$(BUILD_DIR)/%.o: %.c
	gcc -c $< -o $@ $(CPPFLAGS)

# Link step: .o to executable
$(BUILD_DIR)/$(NAME): $(OBJ)
	gcc $^ -o $@ $(LDFLAGS)

run: $(BUILD_DIR)/$(NAME)
	./$(BUILD_DIR)/$(NAME) -c .config -V

clean:
	rm -rf $(BUILD_DIR)

