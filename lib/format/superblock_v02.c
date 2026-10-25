// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (C) 2026 Cristian Recabarren
 */

#include "codec_internal.h"

#include <mfs/allocation.h>
#include <mfs/format.h>

#include <stddef.h>
#include <stdint.h>

/*
 * Experimental MFS on-disk format v0.2.
 *
 * The v0.2 revision introduces persistent allocation
 * bitmap descriptors while preserving the common
 * superblock field offsets and encoding conventions.
 *
 * The v0.1 codec remains independent and unchanged.
 */

enum mfs_format_result
mfs_superblock_v02_encode(
    uint8_t *buffer,
    size_t buffer_size,
    const struct mfs_superblock_v02 *superblock,
    uint64_t volume_bytes)
{
    static const uint8_t magic[8] = {
        'M', 'Y', 'O', 'S', 'M', 'F', 'S', 0
    };

    /*
     * Validate the output buffer and input object.
     */
    if (buffer == NULL || superblock == NULL)
        return MFS_FORMAT_INVALID_ARGUMENT;

    if (buffer_size < MFS_BLOCK_SIZE)
        return MFS_FORMAT_INVALID_ARGUMENT;

    /*
     * Validate the common volume capacity contract.
     *
     * This also prevents overflow when computing
     * total_blocks * MFS_BLOCK_SIZE.
     */
    if (!mfs_format_valid_volume_geometry(
            superblock->total_blocks,
            volume_bytes))
        return MFS_FORMAT_INVALID_ARGUMENT;

    /*
     * Calculate the v0.2 allocation geometry.
     *
     * Unlike v0.1, this revision requires at least
     * four physical blocks.
     */
    struct mfs_allocation_geometry geometry = {0};

    if (mfs_allocation_geometry(
            superblock->total_blocks,
            &geometry) != MFS_ALLOCATION_OK)
        return MFS_FORMAT_INVALID_ARGUMENT;

    /*
     * Allocation metadata must describe precisely
     * the extent derived from the volume geometry.
     */
    if (superblock->allocation_start !=
        geometry.bitmap_start)
        return MFS_FORMAT_INVALID_ARGUMENT;

    if (superblock->allocation_blocks !=
        geometry.bitmap_blocks)
        return MFS_FORMAT_INVALID_ARGUMENT;

    /*
     * A volume UUID containing only zero bytes
     * is invalid.
     */
    uint8_t uuid_nonzero = 0;

    for (size_t i = 0; i < MFS_UUID_SIZE; ++i)
        uuid_nonzero |= superblock->volume_uuid[i];

    if (uuid_nonzero == 0)
        return MFS_FORMAT_INVALID_ARGUMENT;

    /*
     * All validation has succeeded.
     *
     * The output buffer must not be modified before
     * this point.
     *
     * Initialize the complete persistent block,
     * including all reserved and inactive regions.
     */
    for (size_t i = 0; i < MFS_BLOCK_SIZE; ++i)
        buffer[i] = 0;

    /*
     * Fixed identification fields.
     */
    for (size_t i = 0; i < sizeof(magic); ++i)
        buffer[i] = magic[i];

    mfs_format_write_le16(
        buffer + 8,
        MFS_FORMAT_MAJOR);

    mfs_format_write_le16(
        buffer + 10,
        MFS_FORMAT_V02_MINOR);

    mfs_format_write_le16(
        buffer + 12,
        MFS_SUPERBLOCK_HEADER_SIZE);

    mfs_format_write_le32(
        buffer + 16,
        MFS_BLOCK_SIZE);

    mfs_format_write_le32(
        buffer + 20,
        MFS_FORMAT_STATE_EXPERIMENTAL);

    /*
     * Volume geometry and identity.
     */
    mfs_format_write_le64(
        buffer + 24,
        superblock->total_blocks);

    for (size_t i = 0; i < MFS_UUID_SIZE; ++i)
        buffer[32 + i] = superblock->volume_uuid[i];

    /*
     * Feature flags, journal descriptors and root
     * inode remain zero in experimental v0.2.
     *
     * Allocation bitmap descriptors occupy their
     * pre-existing superblock field locations.
     */
    mfs_format_write_le64(
        buffer + 96,
        superblock->allocation_start);

    mfs_format_write_le64(
        buffer + 104,
        superblock->allocation_blocks);

    /*
     * Calculate CRC32C over the complete 4096-byte
     * superblock with the checksum field treated
     * as zero.
     */
    uint32_t checksum =
        mfs_format_superblock_crc32c(buffer);

    mfs_format_write_le32(
        buffer + MFS_SUPERBLOCK_CRC_OFFSET,
        checksum);

    return MFS_FORMAT_OK;
}

/*
 * Decode and validate an experimental MFS v0.2
 * superblock.
 *
 * The decoder validates the persistent representation
 * without reading the allocation bitmap itself.
 *
 * On failure, the output structure remains unchanged.
 */

enum mfs_format_result
mfs_superblock_v02_decode(
    const uint8_t *buffer,
    size_t buffer_size,
    uint64_t volume_bytes,
    struct mfs_superblock_v02 *superblock)
{
    static const uint8_t magic[8] = {
        'M', 'Y', 'O', 'S', 'M', 'F', 'S', 0
    };

    /*
     * Validate input and output arguments.
     */
    if (buffer == NULL || superblock == NULL)
        return MFS_FORMAT_INVALID_ARGUMENT;

    if (buffer_size < MFS_BLOCK_SIZE)
        return MFS_FORMAT_INVALID_ARGUMENT;

    /*
     * Validate the filesystem signature.
     */
    for (size_t i = 0; i < sizeof(magic); ++i) {
        if (buffer[i] != magic[i])
            return MFS_FORMAT_INVALID_DATA;
    }

    /*
     * Accept only the experimental v0.2 revision.
     */
    if (mfs_format_read_le16(buffer + 8) !=
            MFS_FORMAT_MAJOR ||
        mfs_format_read_le16(buffer + 10) !=
            MFS_FORMAT_V02_MINOR)
        return MFS_FORMAT_UNSUPPORTED_VERSION;

    /*
     * Validate the fixed superblock fields.
     */
    if (mfs_format_read_le16(buffer + 12) !=
            MFS_SUPERBLOCK_HEADER_SIZE ||
        mfs_format_read_le16(buffer + 14) != 0 ||
        mfs_format_read_le32(buffer + 16) !=
            MFS_BLOCK_SIZE ||
        mfs_format_read_le32(buffer + 20) !=
            MFS_FORMAT_STATE_EXPERIMENTAL)
        return MFS_FORMAT_INVALID_DATA;

    /*
     * Feature flags, journal descriptors and root
     * inode are not implemented in revision v0.2.
     *
     * Their persistent fields must remain zero.
     */
    for (size_t i = 48; i < 96; ++i) {
        if (buffer[i] != 0)
            return MFS_FORMAT_INVALID_DATA;
    }

    /*
     * Reserved bytes following the checksum must
     * remain zero throughout the complete block.
     */
    for (size_t i = MFS_SUPERBLOCK_CRC_OFFSET + 4;
         i < MFS_BLOCK_SIZE;
         ++i) {
        if (buffer[i] != 0)
            return MFS_FORMAT_INVALID_DATA;
    }

    /*
     * Validate volume capacity and arithmetic bounds.
     */
    uint64_t total_blocks =
        mfs_format_read_le64(buffer + 24);

    if (!mfs_format_valid_volume_geometry(
            total_blocks, volume_bytes))
        return MFS_FORMAT_INVALID_DATA;

    /*
     * Derive the required allocation bitmap geometry.
     *
     * This enforces the v0.2 minimum volume size
     * and verifies that the geometry is representable.
     */
    struct mfs_allocation_geometry geometry = {0};

    if (mfs_allocation_geometry(
            total_blocks,
            &geometry) != MFS_ALLOCATION_OK)
        return MFS_FORMAT_INVALID_DATA;

    /*
     * Decode the persistent allocation descriptors.
     */
    uint64_t allocation_start =
        mfs_format_read_le64(buffer + 96);

    uint64_t allocation_blocks =
        mfs_format_read_le64(buffer + 104);

    /*
     * Both descriptors must match the geometry
     * derived from the actual volume capacity.
     */
    if (allocation_start != geometry.bitmap_start)
        return MFS_FORMAT_INVALID_DATA;

    if (allocation_blocks != geometry.bitmap_blocks)
        return MFS_FORMAT_INVALID_DATA;

    /*
     * Validate the volume UUID.
     *
     * The all-zero UUID is reserved as invalid.
     */
    uint8_t uuid_nonzero = 0;

    for (size_t i = 0; i < MFS_UUID_SIZE; ++i)
        uuid_nonzero |= buffer[32 + i];

    if (uuid_nonzero == 0)
        return MFS_FORMAT_INVALID_DATA;

    /*
     * Validate CRC32C over the complete superblock.
     *
     * The checksum field is logically zeroed by
     * the shared calculation routine.
     */
    uint32_t stored_crc =
        mfs_format_read_le32(
            buffer + MFS_SUPERBLOCK_CRC_OFFSET);

    uint32_t computed_crc =
        mfs_format_superblock_crc32c(buffer);

    if (stored_crc != computed_crc)
        return MFS_FORMAT_INVALID_CHECKSUM;

    /*
     * All validation has succeeded.
     *
     * Decode into a temporary structure so failures
     * can never publish partially validated metadata.
     */
    struct mfs_superblock_v02 decoded = {0};

    decoded.total_blocks = total_blocks;
    decoded.allocation_start = allocation_start;
    decoded.allocation_blocks = allocation_blocks;

    for (size_t i = 0; i < MFS_UUID_SIZE; ++i)
        decoded.volume_uuid[i] = buffer[32 + i];

    /*
     * Publish only the fully validated structure.
     */
    *superblock = decoded;

    return MFS_FORMAT_OK;
}
