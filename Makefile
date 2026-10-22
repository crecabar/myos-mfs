## SPDX-License-Identifier: GPL-2.0-only
# -----------------------------------------------------------------------------
# Copyright (C) 2026 Cristian Recabarren
# -----------------------------------------------------------------------------

.DEFAULT_GOAL := all
.DELETE_ON_ERROR:

include config.mk

# -----------------------------------------------------------------------------
# Directories
# -----------------------------------------------------------------------------

BUILD_DIR := build
OBJ_DIR := $(BUILD_DIR)/obj
BIN_DIR := $(BUILD_DIR)/bin
LIB_DIR := $(BUILD_DIR)/lib
TEST_DIR := $(BUILD_DIR)/tests

# -----------------------------------------------------------------------------
# Source discovery
# -----------------------------------------------------------------------------

LIB_SRCS := $(shell find lib -type f -name '*.c' | sort)
LIB_OBJS := $(patsubst lib/%.c,$(OBJ_DIR)/lib/%.o,$(LIB_SRCS))

TEST_SRCS := $(shell find tests -type f -name '*_test.c' | sort)
TEST_NAMES := $(notdir $(TEST_SRCS:.c=))
TEST_BINS := $(addprefix $(TEST_DIR)/,$(TEST_NAMES))

LIBMFS := $(LIB_DIR)/libmfs.a

MFS_VERSION_SRC := tools/mfs-version/main.c
MFS_VERSION_OBJ := $(OBJ_DIR)/tools/mfs-version/main.o
MFS_VERSION_BIN := $(BIN_DIR)/mfs-version

# -----------------------------------------------------------------------------
# Compiler configuration
# -----------------------------------------------------------------------------

TEST_CPPFLAGS := $(CPPFLAGS) -Itests/include
TEST_CFLAGS := $(CFLAGS) -Werror -UNDEBUG

# -----------------------------------------------------------------------------
# Public targets
# -----------------------------------------------------------------------------

.PHONY: all lib tools test clean help

all: lib tools

lib: $(LIBMFS)

tools: $(MFS_VERSION_BIN)

test: $(TEST_BINS)
	@set -e; \
	for test_bin in $(TEST_BINS); do \
	    echo "Running $$test_bin"; \
	    ./$$test_bin; \
	done

clean:
	rm -rf $(BUILD_DIR)

help:
	@echo "MFS build targets:"
	@echo "  all    Build MFS library and host tools"
	@echo "  lib    Build libmfs.a"
	@echo "  tools  Build available host tools"
	@echo "  test   Build and run MFS tests"
	@echo "  clean  Remove generated artifacts"
	@echo "  help   Display available targets"

# -----------------------------------------------------------------------------
# Portable library
# -----------------------------------------------------------------------------

$(OBJ_DIR)/lib/%.o: lib/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) -Werror -MMD -MP -c $< -o $@

$(LIBMFS): $(LIB_OBJS)
	@mkdir -p $(dir $@)
	$(AR) rcs $@ $^

# -----------------------------------------------------------------------------
# Host tools
# -----------------------------------------------------------------------------

$(OBJ_DIR)/tools/%.o: tools/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) -MMD -MP -c $< -o $@

$(MFS_VERSION_BIN): $(MFS_VERSION_OBJ)
	@mkdir -p $(dir $@)
	$(CC) $(LDFLAGS) $^ $(LDLIBS) -o $@

# -----------------------------------------------------------------------------
# Tests
# -----------------------------------------------------------------------------

$(OBJ_DIR)/tests/%.o: tests/%.c
	@mkdir -p $(dir $@)
	$(CC) $(TEST_CPPFLAGS) $(TEST_CFLAGS) -MMD -MP -c $< -o $@

$(TEST_DIR)/superblock_test: \
    $(OBJ_DIR)/tests/format/superblock_test.o $(LIBMFS)
	@mkdir -p $(dir $@)
	$(CC) $(LDFLAGS) $^ $(LDLIBS) -o $@

$(TEST_DIR)/block_io_test: \
    $(OBJ_DIR)/tests/io/block_io_test.o $(LIBMFS)
	@mkdir -p $(dir $@)
	$(CC) $(LDFLAGS) $^ $(LDLIBS) -o $@

# -----------------------------------------------------------------------------
# Automatic header dependencies
# -----------------------------------------------------------------------------

DEPS := $(LIB_OBJS:.o=.d) \
        $(MFS_VERSION_OBJ:.o=.d) \
        $(patsubst tests/%.c,$(OBJ_DIR)/tests/%.d,$(TEST_SRCS))

-include $(DEPS)
