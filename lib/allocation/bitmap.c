// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (C) 2026 Cristian Recabarren
 */

#include <mfs/allocation.h>
#include <mfs/format.h>

#include <stddef.h>
#include <stdint.h>

enum mfs_allocation_result
mfs_allocation_bitmap_encode_initial_block(
    uint8_t *buffer,
    size_t buffer_size,
    uint64_t total_blocks,
    uint64_t bitmap_block_index)
{
    if (buffer == NULL || buffer_size < MFS_BLOCK_SIZE)
        return MFS_ALLOCATION_INVALID_ARGUMENT;

    struct mfs_allocation_geometry geometry = {0};

    enum mfs_allocation_result result =
        mfs_allocation_geometry(total_blocks, &geometry);

    if (result != MFS_ALLOCATION_OK)
        return result;

    if (bitmap_block_index >= geometry.bitmap_blocks)
        return MFS_ALLOCATION_INVALID_ARGUMENT;

    /*
     * The bitmap index is relative to the bitmap extent.
     * Each bitmap block represents 32768 physical blocks.
     *
     * Geometry validation bounds the arithmetic below.
     */
    const uint64_t first_block =
        bitmap_block_index * MFS_ALLOCATION_BITS_PER_BLOCK;

    for (size_t i = 0; i < MFS_BLOCK_SIZE; i++) {
        const uint64_t byte_first_block =
            first_block + (uint64_t)i * 8U;

        uint8_t value = 0;

        for (unsigned int bit = 0; bit < 8; bit++) {
            const uint64_t physical_block =
                byte_first_block + bit;

            if (physical_block < geometry.first_free_block ||
                physical_block >= geometry.total_blocks) {
                value |= (uint8_t)(1U << bit);
            }
        }

        buffer[i] = value;
    }

    return MFS_ALLOCATION_OK;
}
