// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (C) 2026 Cristian Recabarren
 */
#ifndef MFS_ALLOCATION_H
#define MFS_ALLOCATION_H

#include <stddef.h>
#include <stdint.h>

/*
 * Experimental MFS v0.2 allocation geometry.
 *
 * These structures are not an on-disk ABI.
 */

#define MFS_ALLOCATION_BITMAP_START 2U
#define MFS_ALLOCATION_BITS_PER_BLOCK 32768U
#define MFS_ALLOCATION_MIN_BLOCKS 4U

enum mfs_allocation_result {
    MFS_ALLOCATION_OK = 0,
    MFS_ALLOCATION_INVALID_ARGUMENT,
    MFS_ALLOCATION_INVALID_GEOMETRY
};

struct mfs_allocation_geometry {
    uint64_t total_blocks;
    uint64_t bitmap_start;
    uint64_t bitmap_blocks;
    uint64_t first_free_block;
    uint64_t free_blocks;
};

/*
 * Calculate the initial allocation bitmap geometry.
 *
 * On failure, the output structure remains unchanged.
 *
 * No I/O, allocation or platform services are required.
 */
enum mfs_allocation_result mfs_allocation_geometry(
    uint64_t total_blocks,
    struct mfs_allocation_geometry *geometry
);

/*
 * Encode one block of the initial allocation bitmap.
 *
 * bitmap_block_index is zero-based relative to the
 * allocation bitmap extent, not the physical volume.
 *
 * The output buffer must hold at least MFS_BLOCK_SIZE
 * bytes. On success, exactly one bitmap block is encoded.
 *
 * On failure, the output buffer remains unchanged.
 *
 * No I/O, heap allocation or platform services are required.
 */
enum mfs_allocation_result mfs_allocation_bitmap_encode_initial_block(
    uint8_t *buffer,
    size_t buffer_size,
    uint64_t total_blocks,
    uint64_t bitmap_block_index
);

#endif /* MFS_ALLOCATION_H */
