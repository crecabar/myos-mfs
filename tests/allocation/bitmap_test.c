// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (C) 2026 Cristian Recabarren
 */

#include <mfs/allocation.h>
#include <mfs/assert.h>
#include <mfs/format.h>

#include <stdint.h>
#include <stdio.h>
#include <string.h>

struct bitmap_vector {
    uint64_t total_blocks;
    uint64_t bitmap_index;
    uint8_t first_byte;
    uint8_t remaining_byte;
};

static void check_rejected_encoding(
    uint64_t total_blocks,
    uint64_t bitmap_index,
    size_t buffer_size,
    enum mfs_allocation_result expected
)
{
    uint8_t buffer[MFS_BLOCK_SIZE];

    memset(buffer, 0xA5, sizeof(buffer));

    MFS_TEST_ASSERT(
        mfs_allocation_bitmap_encode_initial_block(
            buffer,
            buffer_size,
            total_blocks,
            bitmap_index
        ) == expected
    );

    for (size_t i = 0; i < sizeof(buffer); i++)
        MFS_TEST_ASSERT(buffer[i] == 0xA5);
}

static void test_reference_vectors(void)
{
    static const struct bitmap_vector vectors[] = {
        {4,     0, 0xF7, 0xFF},
        {32768, 0, 0x07, 0x00},
        {32769, 0, 0x0F, 0x00},
        {32769, 1, 0xFE, 0xFF},
        {65536, 0, 0x0F, 0x00},
        {65536, 1, 0x00, 0x00},
        {65537, 0, 0x1F, 0x00},
        {65537, 1, 0x00, 0x00},
        {65537, 2, 0xFE, 0xFF}
    };

    uint8_t buffer[MFS_BLOCK_SIZE];

    for (size_t i = 0; i < sizeof(vectors) / sizeof(vectors[0]); i++) {
        memset(buffer, 0xA5, sizeof(buffer));

        MFS_TEST_ASSERT(
            mfs_allocation_bitmap_encode_initial_block(
                buffer,
                sizeof(buffer),
                vectors[i].total_blocks,
                vectors[i].bitmap_index
            ) == MFS_ALLOCATION_OK
        );

        MFS_TEST_ASSERT(buffer[0] == vectors[i].first_byte);

        for (size_t j = 1; j < sizeof(buffer); j++) {
            MFS_TEST_ASSERT(
                buffer[j] == vectors[i].remaining_byte
            );
        }
    }

    printf("PASS: initial bitmap reference vectors\n");
}

static void test_invalid_arguments(void)
{
    MFS_TEST_ASSERT(
        mfs_allocation_bitmap_encode_initial_block(
            NULL, MFS_BLOCK_SIZE, 4, 0
        ) == MFS_ALLOCATION_INVALID_ARGUMENT
    );

    check_rejected_encoding(
        4, 0, 0,
        MFS_ALLOCATION_INVALID_ARGUMENT
    );

    check_rejected_encoding(
        4, 0, MFS_BLOCK_SIZE - 1,
        MFS_ALLOCATION_INVALID_ARGUMENT
    );

    check_rejected_encoding(
        4, 1, MFS_BLOCK_SIZE,
        MFS_ALLOCATION_INVALID_ARGUMENT
    );

    check_rejected_encoding(
        32769, 2, MFS_BLOCK_SIZE,
        MFS_ALLOCATION_INVALID_ARGUMENT
    );

    check_rejected_encoding(
        4, UINT64_MAX, MFS_BLOCK_SIZE,
        MFS_ALLOCATION_INVALID_ARGUMENT
    );

    check_rejected_encoding(
        3, 0, MFS_BLOCK_SIZE,
        MFS_ALLOCATION_INVALID_GEOMETRY
    );

    check_rejected_encoding(
        UINT64_MAX, 0, MFS_BLOCK_SIZE,
        MFS_ALLOCATION_INVALID_GEOMETRY
    );

    printf("PASS: invalid bitmap arguments rejected\n");
    printf("PASS: bitmap output preserved on failure\n");
}

static void test_oversized_buffer(void)
{
    uint8_t buffer[MFS_BLOCK_SIZE + 16];

    memset(buffer, 0xA5, sizeof(buffer));

    MFS_TEST_ASSERT(
        mfs_allocation_bitmap_encode_initial_block(
            buffer, sizeof(buffer), 4, 0
        ) == MFS_ALLOCATION_OK
    );

    MFS_TEST_ASSERT(buffer[0] == 0xF7);

    for (size_t i = 1; i < MFS_BLOCK_SIZE; i++)
        MFS_TEST_ASSERT(buffer[i] == 0xFF);

    for (size_t i = MFS_BLOCK_SIZE; i < sizeof(buffer); i++)
        MFS_TEST_ASSERT(buffer[i] == 0xA5);

    printf("PASS: oversized bitmap buffer boundary preserved\n");
}

static void test_maximum_geometry(void)
{
    const uint64_t max_blocks = UINT64_MAX / MFS_BLOCK_SIZE;

    struct mfs_allocation_geometry geometry = {0};
    uint8_t buffer[MFS_BLOCK_SIZE];

    MFS_TEST_ASSERT(
        mfs_allocation_geometry(max_blocks, &geometry) ==
        MFS_ALLOCATION_OK
    );

    memset(buffer, 0xA5, sizeof(buffer));

    MFS_TEST_ASSERT(
        mfs_allocation_bitmap_encode_initial_block(
            buffer,
            sizeof(buffer),
            max_blocks,
            geometry.bitmap_blocks - 1
        ) == MFS_ALLOCATION_OK
    );

    for (size_t i = 0; i < MFS_BLOCK_SIZE - 1; i++)
        MFS_TEST_ASSERT(buffer[i] == 0x00);

    MFS_TEST_ASSERT(buffer[MFS_BLOCK_SIZE - 1] == 0x80);

    printf("PASS: maximum bitmap geometry boundary\n");
}

int main(void)
{
    test_reference_vectors();
    test_invalid_arguments();
    test_oversized_buffer();
    test_maximum_geometry();

    printf("All allocation bitmap tests passed.\n");
    return 0;
}
