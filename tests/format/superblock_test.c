// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (C) 2026 Cristian Recabarren
 */
#include <mfs/format.h>
#include <mfs/assert.h>

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#define TEST_VOLUME_BLOCKS UINT64_C(16384)
#define TEST_VOLUME_BYTES \
    (TEST_VOLUME_BLOCKS * MFS_BLOCK_SIZE)


static void make_valid_superblock(uint8_t *buffer)
{
    struct mfs_superblock sb = {
        .total_blocks = TEST_VOLUME_BLOCKS
    };

    for (size_t i = 0; i < MFS_UUID_SIZE; ++i)
        sb.volume_uuid[i] = (uint8_t)i;

    MFS_TEST_ASSERT(mfs_superblock_encode(
        buffer, MFS_BLOCK_SIZE, &sb,
        TEST_VOLUME_BYTES) == MFS_FORMAT_OK);
}

static void test_superblock_encode(void)
{
    uint8_t buffer[MFS_BLOCK_SIZE];

    struct mfs_superblock sb = {
        .total_blocks = TEST_VOLUME_BLOCKS
    };

    for (size_t i = 0; i < MFS_UUID_SIZE; ++i)
        sb.volume_uuid[i] = (uint8_t)i;

    enum mfs_format_result result =
        mfs_superblock_encode(buffer, sizeof(buffer),
                              &sb, TEST_VOLUME_BYTES);

    MFS_TEST_ASSERT(result == MFS_FORMAT_OK);

    static const uint8_t expected_magic[8] = {
        'M', 'Y', 'O', 'S', 'M', 'F', 'S', 0
    };

    for (size_t i = 0; i < sizeof(expected_magic); ++i)
        MFS_TEST_ASSERT(buffer[i] == expected_magic[i]);

    /*
     * CRC32C expected by docs/on-disk-format.md:
     *
     * 0x0737580F, stored little-endian.
     */

    MFS_TEST_ASSERT(buffer[112] == 0x0F);
    MFS_TEST_ASSERT(buffer[113] == 0x58);
    MFS_TEST_ASSERT(buffer[114] == 0x37);
    MFS_TEST_ASSERT(buffer[115] == 0x07);

    printf("PASS: superblock encode\n");
}

static void test_superblock_roundtrip(void)
{
    uint8_t buffer[MFS_BLOCK_SIZE];

    struct mfs_superblock original = {
        .total_blocks = TEST_VOLUME_BLOCKS
    };

    for (size_t i = 0; i < MFS_UUID_SIZE; ++i)
        original.volume_uuid[i] = (uint8_t)i;

    MFS_TEST_ASSERT(mfs_superblock_encode(
        buffer, sizeof(buffer), &original,
        TEST_VOLUME_BYTES) == MFS_FORMAT_OK);

    struct mfs_superblock decoded = {0};

    MFS_TEST_ASSERT(mfs_superblock_decode(
        buffer, sizeof(buffer), TEST_VOLUME_BYTES,
        &decoded) == MFS_FORMAT_OK);

    MFS_TEST_ASSERT(decoded.total_blocks == original.total_blocks);

    for (size_t i = 0; i < MFS_UUID_SIZE; ++i)
        MFS_TEST_ASSERT(decoded.volume_uuid[i] == original.volume_uuid[i]);

    printf("PASS: superblock roundtrip\n");
}


static void test_invalid_magic(void)
{
    uint8_t buffer[MFS_BLOCK_SIZE];
    struct mfs_superblock decoded = {0};

    make_valid_superblock(buffer);

    buffer[0] ^= 0xFF;

    MFS_TEST_ASSERT(mfs_superblock_decode(
        buffer, sizeof(buffer), TEST_VOLUME_BYTES,
        &decoded) == MFS_FORMAT_INVALID_DATA);

    printf("PASS: invalid magic\n");
}

static void test_unsupported_version(void)
{
    uint8_t buffer[MFS_BLOCK_SIZE];
    struct mfs_superblock decoded = {0};

    make_valid_superblock(buffer);

    buffer[10] = 2;

    MFS_TEST_ASSERT(mfs_superblock_decode(
        buffer, sizeof(buffer), TEST_VOLUME_BYTES,
        &decoded) == MFS_FORMAT_UNSUPPORTED_VERSION);

    printf("PASS: unsupported version\n");
}

static void test_invalid_checksum(void)
{
    uint8_t buffer[MFS_BLOCK_SIZE];
    struct mfs_superblock decoded = {0};

    make_valid_superblock(buffer);

    buffer[32] ^= 0x01;

    MFS_TEST_ASSERT(mfs_superblock_decode(
        buffer, sizeof(buffer), TEST_VOLUME_BYTES,
        &decoded) == MFS_FORMAT_INVALID_CHECKSUM);

    printf("PASS: invalid checksum\n");
}

static void test_invalid_geometry(void)
{
    uint8_t buffer[MFS_BLOCK_SIZE];
    struct mfs_superblock decoded = {0};

    make_valid_superblock(buffer);

    MFS_TEST_ASSERT(mfs_superblock_decode(
        buffer, sizeof(buffer),
        TEST_VOLUME_BYTES - MFS_BLOCK_SIZE,
        &decoded) == MFS_FORMAT_INVALID_DATA);

    printf("PASS: invalid geometry\n");
}

static void test_encode_invalid_arguments(void)
{
    uint8_t buffer[MFS_BLOCK_SIZE];

    struct mfs_superblock sb = {
        .total_blocks = TEST_VOLUME_BLOCKS
    };

    for (size_t i = 0; i < MFS_UUID_SIZE; ++i)
        sb.volume_uuid[i] = (uint8_t)i;

    /*
     * Fill the output buffer with a recognizable pattern.
     */

    for (size_t i = 0; i < sizeof(buffer); ++i)
        buffer[i] = 0xA5;

    MFS_TEST_ASSERT(mfs_superblock_encode(
        buffer, sizeof(buffer) - 1, &sb,
        TEST_VOLUME_BYTES) == MFS_FORMAT_INVALID_ARGUMENT);

    /*
     * Failed encoding must not alter the output buffer.
     */

    for (size_t i = 0; i < sizeof(buffer); ++i)
        MFS_TEST_ASSERT(buffer[i] == 0xA5);

    MFS_TEST_ASSERT(mfs_superblock_encode(
        buffer, sizeof(buffer), &sb,
        TEST_VOLUME_BYTES - MFS_BLOCK_SIZE)
        == MFS_FORMAT_INVALID_ARGUMENT);

    for (size_t i = 0; i < sizeof(buffer); ++i)
        MFS_TEST_ASSERT(buffer[i] == 0xA5);

    printf("PASS: encode invalid arguments\n");
}

static void test_decode_preserves_output_on_failure(void)
{
    uint8_t buffer[MFS_BLOCK_SIZE];

    struct mfs_superblock decoded = {
        .total_blocks = UINT64_C(0xDEADBEEF)
    };

    for (size_t i = 0; i < MFS_UUID_SIZE; ++i)
        decoded.volume_uuid[i] = 0xA5;

    make_valid_superblock(buffer);

    /*
     * Damage the checksum without changing the remaining
     * superblock fields.
     */

    buffer[MFS_SUPERBLOCK_CRC_OFFSET] ^= 0x01;

    MFS_TEST_ASSERT(mfs_superblock_decode(
        buffer, sizeof(buffer), TEST_VOLUME_BYTES,
        &decoded) == MFS_FORMAT_INVALID_CHECKSUM);

    MFS_TEST_ASSERT(decoded.total_blocks == UINT64_C(0xDEADBEEF));

    for (size_t i = 0; i < MFS_UUID_SIZE; ++i)
        MFS_TEST_ASSERT(decoded.volume_uuid[i] == 0xA5);

    printf("PASS: decode preserves output on failure\n");
}

static void test_nonzero_reserved_bytes(void)
{
    uint8_t buffer[MFS_BLOCK_SIZE];
    struct mfs_superblock decoded = {0};

    make_valid_superblock(buffer);

    buffer[128] = 0x01;

    MFS_TEST_ASSERT(mfs_superblock_decode(
        buffer, sizeof(buffer), TEST_VOLUME_BYTES,
        &decoded) == MFS_FORMAT_INVALID_DATA);

    printf("PASS: nonzero reserved bytes\n");
}

static void test_superblock_golden_fixture(void)
{
    uint8_t actual[MFS_BLOCK_SIZE];
    uint8_t expected[MFS_BLOCK_SIZE] = {0};

    static const uint8_t header_prefix[48] = {
        0x4D, 0x59, 0x4F, 0x53, 0x4D, 0x46, 0x53, 0x00,
        0x00, 0x00, 0x01, 0x00, 0x80, 0x00, 0x00, 0x00,
        0x00, 0x10, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00,
        0x00, 0x40, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
        0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F
    };

    static const uint8_t expected_crc[4] = {
        0x0F, 0x58, 0x37, 0x07
    };

    for (size_t i = 0; i < sizeof(header_prefix); ++i)
        expected[i] = header_prefix[i];

    for (size_t i = 0; i < sizeof(expected_crc); ++i)
        expected[MFS_SUPERBLOCK_CRC_OFFSET + i] =
            expected_crc[i];

    make_valid_superblock(actual);

    for (size_t i = 0; i < MFS_BLOCK_SIZE; ++i) {
        MFS_TEST_ASSERT(actual[i] == expected[i]);
    }

    printf("PASS: superblock golden fixture\n");
}

static void test_zero_uuid(void)
{
    uint8_t buffer[MFS_BLOCK_SIZE];
    struct mfs_superblock sb = {
        .total_blocks = TEST_VOLUME_BLOCKS
    };

    /*
     * The UUID is all zero because the structure
     * was zero-initialized.
     */

    MFS_TEST_ASSERT(mfs_superblock_encode(
        buffer, sizeof(buffer), &sb,
        TEST_VOLUME_BYTES) == MFS_FORMAT_INVALID_ARGUMENT);

    printf("PASS: zero UUID rejected\n");
}

static void test_unsupported_features(void)
{
    uint8_t buffer[MFS_BLOCK_SIZE];
    struct mfs_superblock decoded = {0};

    make_valid_superblock(buffer);

    /*
     * compat_features begins at offset 48.
     * All feature masks must be zero in v0.1.
     */

    buffer[48] = 1;

    MFS_TEST_ASSERT(mfs_superblock_decode(
        buffer, sizeof(buffer), TEST_VOLUME_BYTES,
        &decoded) == MFS_FORMAT_INVALID_DATA);

    printf("PASS: unsupported features rejected\n");
}

static void test_truncated_buffer(void)
{
    uint8_t buffer[MFS_BLOCK_SIZE];
    struct mfs_superblock decoded = {0};

    make_valid_superblock(buffer);

    MFS_TEST_ASSERT(mfs_superblock_decode(
        buffer, MFS_BLOCK_SIZE - 1,
        TEST_VOLUME_BYTES,
        &decoded) == MFS_FORMAT_INVALID_ARGUMENT);

    printf("PASS: truncated buffer rejected\n");
}

static void test_volume_geometry_overflow(void)
{
    uint8_t buffer[MFS_BLOCK_SIZE];

    struct mfs_superblock sb = {
        .total_blocks = UINT64_MAX
    };

    for (size_t i = 0; i < MFS_UUID_SIZE; ++i)
        sb.volume_uuid[i] = (uint8_t)i;

    MFS_TEST_ASSERT(mfs_superblock_encode(
        buffer, sizeof(buffer), &sb,
        UINT64_MAX) == MFS_FORMAT_INVALID_ARGUMENT);

    printf("PASS: volume geometry overflow rejected\n");
}


static void test_null_arguments(void)
{
    uint8_t buffer[MFS_BLOCK_SIZE];
    struct mfs_superblock sb = {0};

    MFS_TEST_ASSERT(mfs_superblock_encode(
        NULL, MFS_BLOCK_SIZE, &sb,
        TEST_VOLUME_BYTES) == MFS_FORMAT_INVALID_ARGUMENT);

    MFS_TEST_ASSERT(mfs_superblock_encode(
        buffer, sizeof(buffer), NULL,
        TEST_VOLUME_BYTES) == MFS_FORMAT_INVALID_ARGUMENT);

    MFS_TEST_ASSERT(mfs_superblock_decode(
        NULL, MFS_BLOCK_SIZE, TEST_VOLUME_BYTES,
        &sb) == MFS_FORMAT_INVALID_ARGUMENT);

    MFS_TEST_ASSERT(mfs_superblock_decode(
        buffer, sizeof(buffer), TEST_VOLUME_BYTES,
        NULL) == MFS_FORMAT_INVALID_ARGUMENT);

    printf("PASS: null arguments rejected\n");
}

static void test_decode_zero_uuid(void)
{
    uint8_t buffer[MFS_BLOCK_SIZE];
    struct mfs_superblock decoded = {0};

    make_valid_superblock(buffer);

    for (size_t i = 0; i < MFS_UUID_SIZE; ++i)
        buffer[32 + i] = 0;

    MFS_TEST_ASSERT(mfs_superblock_decode(
        buffer, sizeof(buffer),
        TEST_VOLUME_BYTES,
        &decoded) == MFS_FORMAT_INVALID_DATA);

    printf("PASS: decode zero UUID rejected\n");
}

static void test_minimum_volume(void)
{
    uint8_t buffer[MFS_BLOCK_SIZE];

    struct mfs_superblock original = {
        .total_blocks = 2
    };

    original.volume_uuid[0] = 1;

    MFS_TEST_ASSERT(mfs_superblock_encode(
        buffer, sizeof(buffer),
        &original,
        2 * MFS_BLOCK_SIZE) == MFS_FORMAT_OK);

    struct mfs_superblock decoded = {0};

    MFS_TEST_ASSERT(mfs_superblock_decode(
        buffer, sizeof(buffer),
        2 * MFS_BLOCK_SIZE,
        &decoded) == MFS_FORMAT_OK);

    MFS_TEST_ASSERT(decoded.total_blocks == 2);
    MFS_TEST_ASSERT(decoded.volume_uuid[0] == 1);

    printf("PASS: minimum volume\n");
}

int main(void)
{
    test_superblock_encode();
    test_superblock_roundtrip();

    test_invalid_magic();
    test_unsupported_version();
    test_invalid_checksum();
    test_invalid_geometry();

    test_encode_invalid_arguments();
    test_decode_preserves_output_on_failure();
    test_nonzero_reserved_bytes();

    test_superblock_golden_fixture();

    test_zero_uuid();
    test_unsupported_features();
    test_truncated_buffer();
    test_volume_geometry_overflow();
    test_null_arguments();

    test_decode_zero_uuid();
    test_minimum_volume();

    printf("All superblock tests passed.\n");

    return 0;
}
