// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (C) 2026 Cristian Recabarren
 */
#ifndef MFS_FORMAT_H
#define MFS_FORMAT_H

#include <stddef.h>
#include <stdint.h>

/*
 * Experimental MFS on-disk format v0.1.
 *
 * These constants describe the persistent format,
 * independently of the MFS implementation version.
 */

#define MFS_FORMAT_MAJOR 0
#define MFS_FORMAT_MINOR 1

#define MFS_FORMAT_V02_MINOR 2U

#define MFS_BLOCK_SIZE 4096U
#define MFS_SUPERBLOCK_BLOCK 1U
#define MFS_SUPERBLOCK_OFFSET 4096U
#define MFS_SUPERBLOCK_HEADER_SIZE 128U

#define MFS_UUID_SIZE 16U

#define MFS_FORMAT_STATE_EXPERIMENTAL 1U

/*
 * Superblock checksum location relative to
 * the beginning of the superblock.
 */
#define MFS_SUPERBLOCK_CRC_OFFSET 112U

/*
 * Result codes for portable format operations.
 */
enum mfs_format_result {
    MFS_FORMAT_OK = 0,
    MFS_FORMAT_INVALID_ARGUMENT,
    MFS_FORMAT_INVALID_DATA,
    MFS_FORMAT_UNSUPPORTED_VERSION,
    MFS_FORMAT_INVALID_CHECKSUM
};

/*
 * In-memory representation.
 *
 * This structure is not an on-disk ABI.
 * Serialization must use explicit byte offsets.
 */
struct mfs_superblock {
    uint64_t total_blocks;
    uint8_t volume_uuid[MFS_UUID_SIZE];
};

/*
 * The input and output objects must not overlap.
 *
 * On failure, encode leaves the output buffer unchanged.
 * On failure, decode leaves the output structure unchanged.
 */

/*
 * Encode an experimental MFS superblock.
 *
 * The output buffer must hold at least MFS_BLOCK_SIZE
 * bytes. On success, the entire block is initialized.
 *
 * volume_bytes describes the intended volume capacity.
 */
enum mfs_format_result mfs_superblock_encode(
    uint8_t *buffer,
    size_t buffer_size,
    const struct mfs_superblock *superblock,
    uint64_t volume_bytes
);

/*
 * Decode and validate an experimental MFS superblock.
 *
 * The input buffer contains the complete superblock.
 * volume_bytes is the capacity of the selected volume.
 *
 * The output structure is valid only on success.
 */
enum mfs_format_result mfs_superblock_decode(
    const uint8_t *buffer,
    size_t buffer_size,
    uint64_t volume_bytes,
    struct mfs_superblock *superblock
);

/*
 * Experimental MFS on-disk format v0.2.
 *
 * This structure is an in-memory representation,
 * not an on-disk ABI.
 *
 * Allocation metadata must match the geometry
 * derived from total_blocks.
 */
struct mfs_superblock_v02 {
    uint64_t total_blocks;
    uint8_t volume_uuid[MFS_UUID_SIZE];
    uint64_t allocation_start;
    uint64_t allocation_blocks;
};

/*
 * Encode an experimental MFS v0.2 superblock.
 *
 * The output buffer must hold at least MFS_BLOCK_SIZE
 * bytes. On success, the complete block is initialized.
 *
 * The input object and output buffer must not overlap.
 *
 * On failure, the output buffer remains unchanged.
 */
enum mfs_format_result mfs_superblock_v02_encode(
    uint8_t *buffer,
    size_t buffer_size,
    const struct mfs_superblock_v02 *superblock,
    uint64_t volume_bytes
);

/*
 * Decode and validate an experimental MFS v0.2
 * superblock.
 *
 * The input buffer must contain a complete block.
 * volume_bytes is the capacity of the selected volume.
 *
 * The input buffer and output object must not overlap.
 *
 * On failure, the output structure remains unchanged.
 */
enum mfs_format_result mfs_superblock_v02_decode(
    const uint8_t *buffer,
    size_t buffer_size,
    uint64_t volume_bytes,
    struct mfs_superblock_v02 *superblock
);

#endif /* MFS_FORMAT_H */
