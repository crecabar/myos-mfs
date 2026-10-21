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
# Development targets
# -----------------------------------------------------------------------------

.PHONY: tools clean help

tools: $(MFS_VERSION_BIN)

clean:
	rm -rf $(BUILD_DIR)

help:
	@echo "MFS build targets:"
	@echo "  all    Build available host artifacts"
	@echo "  tools  Build available host tools"
	@echo "  clean  Remove generated build artifacts"
	@echo "  help   Display available targets"
