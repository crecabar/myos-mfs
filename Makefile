## SPDX-License-Identifier: GPL-2.0-only
# -----------------------------------------------------------------------------
# Copyright (C) 2026 Cristian Recabarren
# -----------------------------------------------------------------------------

# -----------------------------------------------------------------------------
# MFS Build System
# -----------------------------------------------------------------------------

.DEFAULT_GOAL := all
.DELETE_ON_ERROR:

include config.mk

BUILD_DIR := build
OBJ_DIR := $(BUILD_DIR)/obj
BIN_DIR := $(BUILD_DIR)/bin

MFS_VERSION_SRC := tools/mfs-version/main.c
MFS_VERSION_OBJ := $(OBJ_DIR)/mfs-version/main.o
MFS_VERSION_BIN := $(BIN_DIR)/mfs-version

# -----------------------------------------------------------------------------
# Superblock codec tests
# -----------------------------------------------------------------------------

SUPERBLOCK_SRC := lib/format/superblock.c
SUPERBLOCK_TEST_SRC := tests/format/superblock_test.c

SUPERBLOCK_OBJ := $(OBJ_DIR)/format/superblock.o
SUPERBLOCK_TEST_OBJ := $(OBJ_DIR)/tests/format/superblock_test.o

SUPERBLOCK_TEST_BIN := $(BUILD_DIR)/tests/superblock_test

TEST_CFLAGS := $(CFLAGS) -Werror -UNDEBUG

# -----------------------------------------------------------------------------
# Default target
# -----------------------------------------------------------------------------

.PHONY: all
all: $(MFS_VERSION_BIN)

# -----------------------------------------------------------------------------
# Host tools
# -----------------------------------------------------------------------------

$(MFS_VERSION_OBJ): $(MFS_VERSION_SRC)
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) -MMD -MP -c $< -o $@

$(MFS_VERSION_BIN): $(MFS_VERSION_OBJ)
	@mkdir -p $(dir $@)
	$(CC) $(LDFLAGS) $^ $(LDLIBS) -o $@

-include $(MFS_VERSION_OBJ:.o=.d)

# -----------------------------------------------------------------------------
# Superblock codec
# -----------------------------------------------------------------------------

$(SUPERBLOCK_OBJ): $(SUPERBLOCK_SRC)
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) -Werror -MMD -MP -c $< -o $@

$(SUPERBLOCK_TEST_OBJ): $(SUPERBLOCK_TEST_SRC)
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(TEST_CFLAGS) -MMD -MP -c $< -o $@

$(SUPERBLOCK_TEST_BIN): $(SUPERBLOCK_OBJ) $(SUPERBLOCK_TEST_OBJ)
	@mkdir -p $(dir $@)
	$(CC) $(LDFLAGS) $^ $(LDLIBS) -o $@

-include $(SUPERBLOCK_OBJ:.o=.d)
-include $(SUPERBLOCK_TEST_OBJ:.o=.d)

# -----------------------------------------------------------------------------
# Development targets
# -----------------------------------------------------------------------------

.PHONY: tools test clean help

tools: $(MFS_VERSION_BIN)

test: $(SUPERBLOCK_TEST_BIN)
	./$(SUPERBLOCK_TEST_BIN)

clean:
	rm -rf $(BUILD_DIR)

help:
	@echo "MFS build targets:"
	@echo "  all    Build available host artifacts"
	@echo "  tools  Build available host tools"
	@echo "  test   Build and run MFS tests"
	@echo "  clean  Remove generated build artifacts"
	@echo "  help   Display available targets"
