// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (C) 2026 Cristian Recabarren
 */

#include <mfs/allocation.h>
#include <mfs/format.h>

#include <stddef.h>
#include <stdint.h>

enum mfs_allocation_result
mfs_allocation_geometry(uint64_t total_blocks,
                        struct mfs_allocation_geometry *geometry)
{
    if (geometry == NULL)
        return MFS_ALLOCATION_INVALID_ARGUMENT;

    if (total_blocks < MFS_ALLOCATION_MIN_BLOCKS)
        return MFS_ALLOCATION_INVALID_GEOMETRY;

    /*
     * Physical volume offsets must fit in uint64_t.
     */
    if (total_blocks > UINT64_MAX / MFS_BLOCK_SIZE)
        return MFS_ALLOCATION_INVALID_GEOMETRY;

    const uint64_t bits_per_block =
        MFS_ALLOCATION_BITS_PER_BLOCK;

    uint64_t bitmap_blocks = total_blocks / bits_per_block;

    if (total_blocks % bits_per_block != 0)
        bitmap_blocks++;

    /*
     * Ensure that reserved regions and the bitmap fit,
     * leaving at least one initially free data block.
     */
    if (bitmap_blocks > total_blocks -
                        MFS_ALLOCATION_BITMAP_START - 1)
        return MFS_ALLOCATION_INVALID_GEOMETRY;

    struct mfs_allocation_geometry result = {0};

    result.total_blocks = total_blocks;
    result.bitmap_start = MFS_ALLOCATION_BITMAP_START;
    result.bitmap_blocks = bitmap_blocks;

    result.first_free_block =
        result.bitmap_start + result.bitmap_blocks;

    result.free_blocks =
        total_blocks - result.first_free_block;

    *geometry = result;

    return MFS_ALLOCATION_OK;
}
