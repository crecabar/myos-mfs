// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (C) 2026 Cristian Recabarren
 */

#include "codec_internal.h"

#include <mfs/format.h>

#include <stdint.h>

/*
 * Experimental MFS on-disk format v0.1.
 *
 * This codec intentionally supports revision 0.1 only.
 * Other revisions are handled by separate codecs.
 */

enum mfs_format_result
mfs_superblock_encode(uint8_t *buffer,
                      size_t buffer_size,
                      const struct mfs_superblock *superblock,
                      uint64_t volume_bytes)
{
    static const uint8_t magic[8] = {
        'M', 'Y', 'O', 'S', 'M', 'F', 'S', 0
    };

    if (buffer == NULL || superblock == NULL)
        return MFS_FORMAT_INVALID_ARGUMENT;

    if (buffer_size < MFS_BLOCK_SIZE)
        return MFS_FORMAT_INVALID_ARGUMENT;

    if (!mfs_format_valid_volume_geometry(
            superblock->total_blocks, volume_bytes))
        return MFS_FORMAT_INVALID_ARGUMENT;

    uint8_t uuid_nonzero = 0;

    for (size_t i = 0; i < MFS_UUID_SIZE; ++i)
        uuid_nonzero |= superblock->volume_uuid[i];

    if (uuid_nonzero == 0)
        return MFS_FORMAT_INVALID_ARGUMENT;

    /*
     * All checks are complete.
     * From this point, initialize the output block.
     */

    for (size_t i = 0; i < MFS_BLOCK_SIZE; ++i)
        buffer[i] = 0;

    for (size_t i = 0; i < sizeof(magic); ++i)
        buffer[i] = magic[i];

    mfs_format_write_le16(buffer + 8, MFS_FORMAT_MAJOR);
    mfs_format_write_le16(buffer + 10, MFS_FORMAT_MINOR);
    mfs_format_write_le16(
        buffer + 12, MFS_SUPERBLOCK_HEADER_SIZE);

    mfs_format_write_le32(buffer + 16, MFS_BLOCK_SIZE);
    mfs_format_write_le32(
        buffer + 20, MFS_FORMAT_STATE_EXPERIMENTAL);

    mfs_format_write_le64(
        buffer + 24, superblock->total_blocks);

    for (size_t i = 0; i < MFS_UUID_SIZE; ++i)
        buffer[32 + i] = superblock->volume_uuid[i];

    /*
     * Feature flags, journal metadata, root inode,
     * allocation metadata and reserved regions remain
     * zero in experimental format v0.1.
     */

    uint32_t checksum =
        mfs_format_superblock_crc32c(buffer);

    mfs_format_write_le32(
        buffer + MFS_SUPERBLOCK_CRC_OFFSET,
        checksum);

    return MFS_FORMAT_OK;
}

enum mfs_format_result
mfs_superblock_decode(const uint8_t *buffer,
                      size_t buffer_size,
                      uint64_t volume_bytes,
                      struct mfs_superblock *superblock)
{
    static const uint8_t magic[8] = {
        'M', 'Y', 'O', 'S', 'M', 'F', 'S', 0
    };

    if (buffer == NULL || superblock == NULL)
        return MFS_FORMAT_INVALID_ARGUMENT;

    if (buffer_size < MFS_BLOCK_SIZE)
        return MFS_FORMAT_INVALID_ARGUMENT;

    /*
     * Validate the fixed identification fields.
     */
    for (size_t i = 0; i < sizeof(magic); ++i) {
        if (buffer[i] != magic[i])
            return MFS_FORMAT_INVALID_DATA;
    }

    if (mfs_format_read_le16(buffer + 8) != MFS_FORMAT_MAJOR ||
        mfs_format_read_le16(buffer + 10) != MFS_FORMAT_MINOR)
        return MFS_FORMAT_UNSUPPORTED_VERSION;

    if (mfs_format_read_le16(buffer + 12) !=
            MFS_SUPERBLOCK_HEADER_SIZE ||
        mfs_format_read_le16(buffer + 14) != 0 ||
        mfs_format_read_le32(buffer + 16) != MFS_BLOCK_SIZE ||
        mfs_format_read_le32(buffer + 20) !=
            MFS_FORMAT_STATE_EXPERIMENTAL)
        return MFS_FORMAT_INVALID_DATA;

    /*
     * Verify inactive and reserved fields.
     * Their bytes must be zero in format v0.1.
     */
    for (size_t i = 48; i < MFS_SUPERBLOCK_CRC_OFFSET; ++i) {
        if (buffer[i] != 0)
            return MFS_FORMAT_INVALID_DATA;
    }

    for (size_t i = MFS_SUPERBLOCK_CRC_OFFSET + 4;
         i < MFS_BLOCK_SIZE;
         ++i) {
        if (buffer[i] != 0)
            return MFS_FORMAT_INVALID_DATA;
    }

    /*
     * Validate volume geometry and identity.
     */
    uint64_t total_blocks = mfs_format_read_le64(buffer + 24);

    if (!mfs_format_valid_volume_geometry(
            total_blocks, volume_bytes))
        return MFS_FORMAT_INVALID_DATA;

    uint8_t uuid_nonzero = 0;

    for (size_t i = 0; i < MFS_UUID_SIZE; ++i)
        uuid_nonzero |= buffer[32 + i];

    if (uuid_nonzero == 0)
        return MFS_FORMAT_INVALID_DATA;

    /*
     * Validate the stored checksum.
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
     * Only now publish the decoded values.
     */
    struct mfs_superblock decoded = {0};

    decoded.total_blocks = total_blocks;

    for (size_t i = 0; i < MFS_UUID_SIZE; ++i)
        decoded.volume_uuid[i] = buffer[32 + i];

    *superblock = decoded;

    return MFS_FORMAT_OK;
}
