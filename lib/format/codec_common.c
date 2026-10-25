// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (C) 2026 Cristian Recabarren
 */

#include "codec_internal.h"

#include <mfs/format.h>

#include <stddef.h>
#include <stdint.h>

/*
 * Little-endian decoding.
 */

uint16_t mfs_format_read_le16(const uint8_t *p)
{
    return (uint16_t)p[0]
         | (uint16_t)((uint16_t)p[1] << 8);
}

uint32_t mfs_format_read_le32(const uint8_t *p)
{
    return (uint32_t)p[0]
         | ((uint32_t)p[1] << 8)
         | ((uint32_t)p[2] << 16)
         | ((uint32_t)p[3] << 24);
}

uint64_t mfs_format_read_le64(const uint8_t *p)
{
    return (uint64_t)mfs_format_read_le32(p)
         | ((uint64_t)mfs_format_read_le32(p + 4) << 32);
}

/*
 * Little-endian encoding.
 */

void mfs_format_write_le16(uint8_t *p, uint16_t value)
{
    p[0] = (uint8_t)value;
    p[1] = (uint8_t)(value >> 8);
}

void mfs_format_write_le32(uint8_t *p, uint32_t value)
{
    p[0] = (uint8_t)value;
    p[1] = (uint8_t)(value >> 8);
    p[2] = (uint8_t)(value >> 16);
    p[3] = (uint8_t)(value >> 24);
}

void mfs_format_write_le64(uint8_t *p, uint64_t value)
{
    mfs_format_write_le32(p, (uint32_t)value);
    mfs_format_write_le32(p + 4, (uint32_t)(value >> 32));
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

/*
 * Calculate the CRC32C of a complete superblock.
 *
 * The checksum field is treated as zero during
 * calculation, regardless of its stored contents.
 *
 * This contract is shared by experimental format
 * revisions v0.1 and v0.2.
 */

uint32_t mfs_format_superblock_crc32c(const uint8_t *block)
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

/*
 * Validate the common volume geometry.
 *
 * The logical volume must contain at least two
 * complete blocks and must match the supplied
 * capacity exactly.
 *
 * This function preserves the original v0.1
 * capacity validation semantics.
 */

int mfs_format_valid_volume_geometry(
    uint64_t total_blocks,
    uint64_t volume_bytes)
{
    if (total_blocks < 2)
        return 0;

    if (total_blocks > UINT64_MAX / MFS_BLOCK_SIZE)
        return 0;

    return total_blocks * MFS_BLOCK_SIZE == volume_bytes;
}
