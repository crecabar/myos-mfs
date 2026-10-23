// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (C) 2026 Cristian Recabarren
 */

#include <mfs/allocation.h>
#include <mfs/format.h>
#include <mfs/assert.h>

#include <stdint.h>
#include <stdio.h>

static void check_geometry(uint64_t total,
                           uint64_t bitmap_blocks,
                           uint64_t first_free,
                           uint64_t free_blocks)
{
    struct mfs_allocation_geometry geometry = {0};

    MFS_TEST_ASSERT(
        mfs_allocation_geometry(total, &geometry) ==
        MFS_ALLOCATION_OK
    );

    MFS_TEST_ASSERT(geometry.total_blocks == total);
    MFS_TEST_ASSERT(geometry.bitmap_start == 2);
    MFS_TEST_ASSERT(geometry.bitmap_blocks == bitmap_blocks);
    MFS_TEST_ASSERT(geometry.first_free_block == first_free);
    MFS_TEST_ASSERT(geometry.free_blocks == free_blocks);
}

static void test_reference_geometries(void)
{
    check_geometry(4, 1, 3, 1);
    check_geometry(32768, 1, 3, 32765);
    check_geometry(32769, 2, 4, 32765);
    check_geometry(65536, 2, 4, 65532);
    check_geometry(65537, 3, 5, 65532);

    printf("PASS: allocation reference geometries\n");
}

static void test_invalid_geometries(void)
{
    struct mfs_allocation_geometry geometry = {0};

    MFS_TEST_ASSERT(
        mfs_allocation_geometry(0, &geometry) ==
        MFS_ALLOCATION_INVALID_GEOMETRY
    );

    MFS_TEST_ASSERT(
        mfs_allocation_geometry(2, &geometry) ==
        MFS_ALLOCATION_INVALID_GEOMETRY
    );

    MFS_TEST_ASSERT(
        mfs_allocation_geometry(3, &geometry) ==
        MFS_ALLOCATION_INVALID_GEOMETRY
    );

    MFS_TEST_ASSERT(
        mfs_allocation_geometry(UINT64_MAX, &geometry) ==
        MFS_ALLOCATION_INVALID_GEOMETRY
    );

    printf("PASS: invalid allocation geometries rejected\n");
}

static void test_output_preserved_on_failure(void)
{
    struct mfs_allocation_geometry geometry = {
        .total_blocks = 100,
        .bitmap_start = 200,
        .bitmap_blocks = 300,
        .first_free_block = 400,
        .free_blocks = 500
    };

    MFS_TEST_ASSERT(
        mfs_allocation_geometry(3, &geometry) ==
        MFS_ALLOCATION_INVALID_GEOMETRY
    );

    MFS_TEST_ASSERT(geometry.total_blocks == 100);
    MFS_TEST_ASSERT(geometry.bitmap_start == 200);
    MFS_TEST_ASSERT(geometry.bitmap_blocks == 300);
    MFS_TEST_ASSERT(geometry.first_free_block == 400);
    MFS_TEST_ASSERT(geometry.free_blocks == 500);

    MFS_TEST_ASSERT(
        mfs_allocation_geometry(4, NULL) ==
        MFS_ALLOCATION_INVALID_ARGUMENT
    );

    printf("PASS: allocation output preserved on failure\n");
}

static void test_maximum_geometry(void)
{
    const uint64_t max_blocks = UINT64_MAX / MFS_BLOCK_SIZE;

    struct mfs_allocation_geometry geometry = {0};

    MFS_TEST_ASSERT(
        mfs_allocation_geometry(max_blocks, &geometry) ==
        MFS_ALLOCATION_OK
    );

    MFS_TEST_ASSERT(geometry.total_blocks == max_blocks);
    MFS_TEST_ASSERT(geometry.bitmap_start == 2);

    MFS_TEST_ASSERT(
        geometry.bitmap_blocks ==
        max_blocks / MFS_ALLOCATION_BITS_PER_BLOCK +
        (max_blocks % MFS_ALLOCATION_BITS_PER_BLOCK != 0)
    );

    MFS_TEST_ASSERT(
        geometry.first_free_block ==
        geometry.bitmap_start + geometry.bitmap_blocks
    );

    MFS_TEST_ASSERT(
        geometry.free_blocks ==
        max_blocks - geometry.first_free_block
    );

    MFS_TEST_ASSERT(geometry.free_blocks > 0);

    MFS_TEST_ASSERT(
        mfs_allocation_geometry(max_blocks + 1, &geometry) ==
        MFS_ALLOCATION_INVALID_GEOMETRY
    );

    MFS_TEST_ASSERT(geometry.total_blocks == max_blocks);

    printf("PASS: maximum allocation geometry boundaries\n");
}

int main(void)
{
    test_reference_geometries();
    test_invalid_geometries();
    test_output_preserved_on_failure();
    test_maximum_geometry();

    printf("All allocation geometry tests passed.\n");
    return 0;
}
