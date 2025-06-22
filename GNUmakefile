.PHONY: all install-dev uninstall-dev run clean dirs

.ONESHELL:
.DEFAULT_GOAL       := all

SHELL               := /bin/bash

NAME := eeka
BUILD_DIR ?= build

SRC := $(wildcard *.c)
OBJ := $(patsubst %.c,$(BUILD_DIR)/%.o,$(SRC))


CPPFLAGS += -Wall -Wextra -std=c99 -D_GNU_SOURCE -O2
LDFLAGS += -lxcb -lxcb-keysyms -lxcb-xtest

all: $(BUILD_DIR)/$(NAME)

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

$(BUILD_DIR)/%.o: %.c | $(BUILD_DIR)
	gcc -c $< -o $@ $(CPPFLAGS)

$(BUILD_DIR)/$(NAME): $(OBJ)
	gcc $^ -o $@ $(LDFLAGS)

run:
	$(MAKE) clean
	rm -rf .gcc
	$(MAKE) $(BUILD_DIR)/$(NAME) --no-print-directory 2>&1 | tee -a .gcc
	./$(BUILD_DIR)/$(NAME) -c .config -V

clean:
	rm -rf $(BUILD_DIR)

