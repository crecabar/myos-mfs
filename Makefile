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
CORE_TEST_SRCS := $(shell find tests/allocation tests/format tests/io -type f -name '*_test.c' | sort)
CORE_TEST_NAMES := $(notdir $(CORE_TEST_SRCS:.c=))

TEST_BINS := $(addprefix $(TEST_DIR)/,$(CORE_TEST_NAMES)) \
             $(TEST_DIR)/posix_block_device_test \
             $(TEST_DIR)/mkfs_test \
             $(TEST_DIR)/mfsdump_test \
             $(TEST_DIR)/fsck_test

LIBMFS := $(LIB_DIR)/libmfs.a

MFS_VERSION_SRC := tools/mfs-version/main.c
MFS_VERSION_OBJ := $(OBJ_DIR)/tools/mfs-version/main.o
MFS_VERSION_BIN := $(BIN_DIR)/mfs-version

MKFS_SRC := tools/mkfs.mfs/main.c
MKFS_OBJ := $(OBJ_DIR)/tools/mkfs.mfs/main.o
MKFS_BIN := $(BIN_DIR)/mkfs.mfs

MKFS_TEST_SRC := tests/tools/mkfs.mfs/mkfs_test.c
MKFS_TEST_OBJ := $(OBJ_DIR)/tests/tools/mkfs.mfs/mkfs_test.o
MKFS_TEST_BIN := $(TEST_DIR)/mkfs_test

MFSDUMP_SRC := tools/mfsdump/main.c
MFSDUMP_OBJ := $(OBJ_DIR)/tools/mfsdump/main.o
MFSDUMP_BIN := $(BIN_DIR)/mfsdump

MFSDUMP_TEST_SRC := tests/tools/mfsdump/mfsdump_test.c
MFSDUMP_TEST_OBJ := $(OBJ_DIR)/tests/tools/mfsdump/mfsdump_test.o
MFSDUMP_TEST_BIN := $(TEST_DIR)/mfsdump_test

FSCK_SRC := tools/fsck.mfs/main.c
FSCK_OBJ := $(OBJ_DIR)/tools/fsck.mfs/main.o
FSCK_BIN := $(BIN_DIR)/fsck.mfs

FSCK_TEST_SRC := tests/tools/fsck.mfs/fsck_test.c
FSCK_TEST_OBJ := $(OBJ_DIR)/tests/tools/fsck.mfs/fsck_test.o
FSCK_TEST_BIN := $(TEST_DIR)/fsck_test

# -----------------------------------------------------------------------------
# POSIX backend
# -----------------------------------------------------------------------------

POSIX_SRCS := $(shell find platform/posix -type f -name '*.c' | sort)
POSIX_OBJS := $(patsubst platform/posix/%.c,$(OBJ_DIR)/platform/posix/%.o,$(POSIX_SRCS))

POSIX_TEST_SRC := tests/platform/posix/block_device_test.c
POSIX_TEST_OBJ := $(OBJ_DIR)/tests/platform/posix/block_device_test.o
POSIX_TEST_BIN := $(TEST_DIR)/posix_block_device_test

# -----------------------------------------------------------------------------
# Compiler configuration
# -----------------------------------------------------------------------------

TEST_CPPFLAGS := $(CPPFLAGS) -Itests/include
TEST_CFLAGS := $(CFLAGS) -Werror -UNDEBUG

POSIX_CPPFLAGS := $(CPPFLAGS) -Iplatform/posix/include
POSIX_TEST_CPPFLAGS := $(TEST_CPPFLAGS) -Iplatform/posix/include

# -----------------------------------------------------------------------------
# Public targets
# -----------------------------------------------------------------------------

.PHONY: all lib tools test tests clean help

all: lib tools

lib: $(LIBMFS)

tools: $(MFS_VERSION_BIN) $(MKFS_BIN) $(MFSDUMP_BIN) $(FSCK_BIN)

test: $(TEST_BINS) $(MKFS_BIN) $(MFSDUMP_BIN) $(FSCK_BIN)
	@set -e; \
	for test_bin in $(filter-out $(MKFS_TEST_BIN) $(MFSDUMP_TEST_BIN) $(FSCK_TEST_BIN),$(TEST_BINS)); do \
	    echo "Running $$test_bin"; \
	    ./$$test_bin; \
	done
	@echo "Running $(MKFS_TEST_BIN)"
	@./$(MKFS_TEST_BIN) ./$(MKFS_BIN)
	@echo "Running $(MFSDUMP_TEST_BIN)"
	@./$(MFSDUMP_TEST_BIN) ./$(MKFS_BIN) ./$(MFSDUMP_BIN)
	@echo "Running $(FSCK_TEST_BIN)"
	@./$(FSCK_TEST_BIN) ./$(MKFS_BIN) ./$(FSCK_BIN)

tests: test

clean:
	rm -rf $(BUILD_DIR)

help:
	@echo "MFS build targets:"
	@echo "  all    Build MFS library and host tools"
	@echo "  lib    Build libmfs.a"
	@echo "  tools  Build available host tools"
	@echo "  test   Build and run MFS tests"
	@echo "  tests  Alias for test"
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
# POSIX backend
# -----------------------------------------------------------------------------

$(OBJ_DIR)/platform/posix/%.o: platform/posix/%.c
	@mkdir -p $(dir $@)
	$(CC) $(POSIX_CPPFLAGS) $(CFLAGS) -Werror -MMD -MP -c $< -o $@

# -----------------------------------------------------------------------------
# Host tools
# -----------------------------------------------------------------------------

$(OBJ_DIR)/tools/%.o: tools/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) -MMD -MP -c $< -o $@

$(MKFS_OBJ): CPPFLAGS += -Iplatform/posix/include

$(MFS_VERSION_BIN): $(MFS_VERSION_OBJ)
	@mkdir -p $(dir $@)
	$(CC) $(LDFLAGS) $^ $(LDLIBS) -o $@

$(MKFS_BIN): $(MKFS_OBJ) $(POSIX_OBJS) $(LIBMFS)
	@mkdir -p $(dir $@)
	$(CC) $(LDFLAGS) $^ $(LDLIBS) -o $@

$(MFSDUMP_OBJ): CPPFLAGS += -Iplatform/posix/include

$(MFSDUMP_BIN): $(MFSDUMP_OBJ) $(POSIX_OBJS) $(LIBMFS)
	@mkdir -p $(dir $@)
	$(CC) $(LDFLAGS) $^ $(LDLIBS) -o $@

$(FSCK_OBJ): CPPFLAGS += -Iplatform/posix/include

$(FSCK_BIN): $(FSCK_OBJ) $(POSIX_OBJS) $(LIBMFS)
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

$(POSIX_TEST_OBJ): $(POSIX_TEST_SRC)
	@mkdir -p $(dir $@)
	$(CC) $(POSIX_TEST_CPPFLAGS) $(TEST_CFLAGS) -MMD -MP -c $< -o $@

$(POSIX_TEST_BIN): $(POSIX_TEST_OBJ) $(POSIX_OBJS) $(LIBMFS)
	@mkdir -p $(dir $@)
	$(CC) $(LDFLAGS) $^ $(LDLIBS) -o $@

$(MKFS_TEST_OBJ): $(MKFS_TEST_SRC)
	@mkdir -p $(dir $@)
	$(CC) $(POSIX_TEST_CPPFLAGS) $(TEST_CFLAGS) -MMD -MP -c $< -o $@

$(MKFS_TEST_BIN): $(MKFS_TEST_OBJ) $(POSIX_OBJS) $(LIBMFS)
	@mkdir -p $(dir $@)
	$(CC) $(LDFLAGS) $^ $(LDLIBS) -o $@

$(MFSDUMP_TEST_OBJ): $(MFSDUMP_TEST_SRC)
	@mkdir -p $(dir $@)
	$(CC) $(TEST_CPPFLAGS) $(TEST_CFLAGS) -MMD -MP -c $< -o $@

$(MFSDUMP_TEST_BIN): $(MFSDUMP_TEST_OBJ)
	@mkdir -p $(dir $@)
	$(CC) $(LDFLAGS) $^ $(LDLIBS) -o $@

$(FSCK_TEST_OBJ): $(FSCK_TEST_SRC)
	@mkdir -p $(dir $@)
	$(CC) $(TEST_CPPFLAGS) $(TEST_CFLAGS) -MMD -MP -c $< -o $@

$(FSCK_TEST_BIN): $(FSCK_TEST_OBJ)
	@mkdir -p $(dir $@)
	$(CC) $(LDFLAGS) $^ $(LDLIBS) -o $@

$(TEST_DIR)/geometry_test: \
    $(OBJ_DIR)/tests/allocation/geometry_test.o $(LIBMFS)
	@mkdir -p $(dir $@)
	$(CC) $(LDFLAGS) $^ $(LDLIBS) -o $@

# -----------------------------------------------------------------------------
# Automatic header dependencies
# -----------------------------------------------------------------------------

DEPS := $(LIB_OBJS:.o=.d) \
        $(POSIX_OBJS:.o=.d) \
        $(MFS_VERSION_OBJ:.o=.d) \
        $(MKFS_OBJ:.o=.d) \
        $(MFSDUMP_OBJ:.o=.d) \
        $(FSCK_OBJ:.o=.d) \
        $(patsubst tests/%.c,$(OBJ_DIR)/tests/%.d,$(TEST_SRCS))

-include $(DEPS)
