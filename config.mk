# MFS host build configuration

CC = clang

CSTD = -std=c17
WARNINGS = -Wall -Wextra -Wpedantic

CPPFLAGS = -Iinclude
CFLAGS = $(CSTD) $(WARNINGS) -O0 -g
