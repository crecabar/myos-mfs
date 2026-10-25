// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (C) 2026 Cristian Recabarren
 */

#ifndef MFS_FORMAT_CODEC_INTERNAL_H
#define MFS_FORMAT_CODEC_INTERNAL_H

#include <stdint.h>

/*
 * Internal on-disk encoding primitives.
 *
 * These declarations are shared by the format codecs
 * and are not part of the public MFS API.
 */

uint16_t mfs_format_read_le16(const uint8_t *p);
uint32_t mfs_format_read_le32(const uint8_t *p);
uint64_t mfs_format_read_le64(const uint8_t *p);

void mfs_format_write_le16(uint8_t *p, uint16_t value);
void mfs_format_write_le32(uint8_t *p, uint32_t value);
void mfs_format_write_le64(uint8_t *p, uint64_t value);

uint32_t mfs_format_superblock_crc32c(const uint8_t *block);

/*
 * Validate the common volume capacity contract.
 *
 * The minimum supported volume is two blocks.
 * Individual on-disk revisions may impose stricter
 * geometry requirements.
 */
int mfs_format_valid_volume_geometry(
    uint64_t total_blocks,
    uint64_t volume_bytes
);

#endif /* MFS_FORMAT_CODEC_INTERNAL_H */
