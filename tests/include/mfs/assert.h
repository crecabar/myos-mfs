// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (C) 2026 Cristian Recabarren
 */

#ifndef MFS_TEST_ASSERT_H
#define MFS_TEST_ASSERT_H

#include <stdio.h>
#include <stdlib.h>

#define MFS_TEST_ASSERT(condition)                               \
    do {                                                         \
        if (!(condition)) {                                      \
            fprintf(stderr,                                      \
                    "FAIL: %s:%d: %s\n",                         \
                    __FILE__, __LINE__, #condition);             \
            exit(EXIT_FAILURE);                                  \
        }                                                        \
    } while (0)

#endif /* MFS_TEST_ASSERT_H */
