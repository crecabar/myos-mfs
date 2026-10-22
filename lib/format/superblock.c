// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (C) 2026 Cristian Recabarren
 */
#include <mfs/format.h>

#include <stdint.h>

static uint16_t read_le16(const uint8_t *p)
{
    return (uint16_t)p[0]
         | (uint16_t)((uint16_t)p[1] << 8);
}

static uint32_t read_le32(const uint8_t *p)
{
    return (uint32_t)p[0]
         | ((uint32_t)p[1] << 8)
         | ((uint32_t)p[2] << 16)
         | ((uint32_t)p[3] << 24);
}

static uint64_t read_le64(const uint8_t *p)
{
    return (uint64_t)read_le32(p)
         | ((uint64_t)read_le32(p + 4) << 32);
}

static void write_le16(uint8_t *p, uint16_t value)
{
    p[0] = (uint8_t)value;
    p[1] = (uint8_t)(value >> 8);
}

static void write_le32(uint8_t *p, uint32_t value)
{
    p[0] = (uint8_t)value;
    p[1] = (uint8_t)(value >> 8);
    p[2] = (uint8_t)(value >> 16);
    p[3] = (uint8_t)(value >> 24);
}

static void write_le64(uint8_t *p, uint64_t value)
{
    write_le32(p, (uint32_t)value);
    write_le32(p + 4, (uint32_t)(value >> 32));
}

/*
 * CRC-32C (Castagnoli), reflected implementation.
 *
 * Polynomial: 0x82F63B78
 * Initial value: 0xFFFFFFFF
 * Final XOR:     0xFFFFFFFF
 */
static uint32_t crc32c_update(uint32_t crc, uint8_t value)
{
    crc ^= value;

    for (unsigned bit = 0; bit < 8; ++bit) {
        if (crc & 1U)
            crc = (crc >> 1) ^ UINT32_C(0x82F63B78);
        else
            crc >>= 1;
    }

    return crc;
}

static uint32_t superblock_crc32c(const uint8_t *block)
{
    uint32_t crc = UINT32_MAX;

    for (size_t i = 0; i < MFS_BLOCK_SIZE; ++i) {
        uint8_t value = block[i];

        if (i >= MFS_SUPERBLOCK_CRC_OFFSET &&
            i < MFS_SUPERBLOCK_CRC_OFFSET + 4U)
            value = 0;

        crc = crc32c_update(crc, value);
    }

    return crc ^ UINT32_MAX;
}

static int valid_volume_geometry(uint64_t total_blocks,
                                 uint64_t volume_bytes)
{
    if (total_blocks < 2)
        return 0;

    if (total_blocks > UINT64_MAX / MFS_BLOCK_SIZE)
        return 0;

    return total_blocks * MFS_BLOCK_SIZE == volume_bytes;
}


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

    if (!valid_volume_geometry(superblock->total_blocks,
                               volume_bytes))
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

    write_le16(buffer + 8, MFS_FORMAT_MAJOR);
    write_le16(buffer + 10, MFS_FORMAT_MINOR);
    write_le16(buffer + 12, MFS_SUPERBLOCK_HEADER_SIZE);

    write_le32(buffer + 16, MFS_BLOCK_SIZE);
    write_le32(buffer + 20, MFS_FORMAT_STATE_EXPERIMENTAL);

    write_le64(buffer + 24, superblock->total_blocks);

    for (size_t i = 0; i < MFS_UUID_SIZE; ++i)
        buffer[32 + i] = superblock->volume_uuid[i];

    /*
     * Feature flags, journal metadata, root inode,
     * allocation metadata and reserved regions remain
     * zero in experimental format v0.1.
     */

    uint32_t checksum = superblock_crc32c(buffer);

    write_le32(buffer + MFS_SUPERBLOCK_CRC_OFFSET,
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

    if (read_le16(buffer + 8) != MFS_FORMAT_MAJOR ||
        read_le16(buffer + 10) != MFS_FORMAT_MINOR)
        return MFS_FORMAT_UNSUPPORTED_VERSION;

    if (read_le16(buffer + 12) != MFS_SUPERBLOCK_HEADER_SIZE ||
        read_le16(buffer + 14) != 0 ||
        read_le32(buffer + 16) != MFS_BLOCK_SIZE ||
        read_le32(buffer + 20) != MFS_FORMAT_STATE_EXPERIMENTAL)
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
    uint64_t total_blocks = read_le64(buffer + 24);

    if (!valid_volume_geometry(total_blocks, volume_bytes))
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
        read_le32(buffer + MFS_SUPERBLOCK_CRC_OFFSET);

    uint32_t computed_crc = superblock_crc32c(buffer);

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
