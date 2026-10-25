// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (C) 2026 Cristian Recabarren
 */

#include <mfs/assert.h>
#include <mfs/format.h>

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define TEST_VOLUME_BLOCKS UINT64_C(16384)
#define TEST_VOLUME_BYTES \
    (TEST_VOLUME_BLOCKS * MFS_BLOCK_SIZE)

static void check_rejected_decode(
    const uint8_t *buffer,
    size_t buffer_size,
    uint64_t volume_bytes,
    enum mfs_format_result expected)
{
    struct mfs_superblock_v02 decoded = {
        .total_blocks = UINT64_C(0xDEADBEEF),
        .allocation_start = 100,
        .allocation_blocks = 200
    };

    for (size_t i = 0; i < MFS_UUID_SIZE; i++)
        decoded.volume_uuid[i] = 0xA5;

    MFS_TEST_ASSERT(
        mfs_superblock_v02_decode(
            buffer,
            buffer_size,
            volume_bytes,
            &decoded
        ) == expected
    );

    MFS_TEST_ASSERT(
        decoded.total_blocks == UINT64_C(0xDEADBEEF)
    );

    MFS_TEST_ASSERT(decoded.allocation_start == 100);
    MFS_TEST_ASSERT(decoded.allocation_blocks == 200);

    for (size_t i = 0; i < MFS_UUID_SIZE; i++)
        MFS_TEST_ASSERT(decoded.volume_uuid[i] == 0xA5);
}

static void check_rejected_encode(
    const struct mfs_superblock_v02 *superblock,
    size_t buffer_size,
    uint64_t volume_bytes,
    enum mfs_format_result expected)
{
    uint8_t buffer[MFS_BLOCK_SIZE];

    memset(buffer, 0xA5, sizeof(buffer));

    MFS_TEST_ASSERT(
        mfs_superblock_v02_encode(
            buffer,
            buffer_size,
            superblock,
            volume_bytes
        ) == expected
    );

    for (size_t i = 0; i < sizeof(buffer); i++)
        MFS_TEST_ASSERT(buffer[i] == 0xA5);
}

static void make_reference_fixture(uint8_t *buffer)
{
    static const uint8_t prefix[48] = {
        0x4D, 0x59, 0x4F, 0x53, 0x4D, 0x46, 0x53, 0x00,
        0x00, 0x00, 0x02, 0x00, 0x80, 0x00, 0x00, 0x00,
        0x00, 0x10, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00,
        0x00, 0x40, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
        0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F
    };

    memset(buffer, 0, MFS_BLOCK_SIZE);
    memcpy(buffer, prefix, sizeof(prefix));

    /* allocation_start = 2, little-endian. */
    buffer[96] = 0x02;

    /* allocation_blocks = 1, little-endian. */
    buffer[104] = 0x01;

    /* CRC32C = 0x1BDCF879, little-endian. */
    buffer[112] = 0x79;
    buffer[113] = 0xF8;
    buffer[114] = 0xDC;
    buffer[115] = 0x1B;
}

static void make_reference_superblock(
    struct mfs_superblock_v02 *superblock)
{
    memset(superblock, 0, sizeof(*superblock));

    superblock->total_blocks = TEST_VOLUME_BLOCKS;
    superblock->allocation_start = 2;
    superblock->allocation_blocks = 1;

    for (size_t i = 0; i < MFS_UUID_SIZE; i++)
        superblock->volume_uuid[i] = (uint8_t)i;
}

static void test_v02_golden_encoding(void)
{
    uint8_t actual[MFS_BLOCK_SIZE];
    uint8_t expected[MFS_BLOCK_SIZE];

    struct mfs_superblock_v02 superblock;

    make_reference_superblock(&superblock);
    make_reference_fixture(expected);

    memset(actual, 0xA5, sizeof(actual));

    MFS_TEST_ASSERT(
        mfs_superblock_v02_encode(
            actual,
            sizeof(actual),
            &superblock,
            TEST_VOLUME_BYTES
        ) == MFS_FORMAT_OK
    );

    MFS_TEST_ASSERT(
        memcmp(actual, expected, sizeof(actual)) == 0
    );

    printf("PASS: v0.2 superblock golden encoding\n");
}

static void test_v02_golden_decoding(void)
{
    uint8_t fixture[MFS_BLOCK_SIZE];

    struct mfs_superblock_v02 decoded = {0};

    make_reference_fixture(fixture);

    MFS_TEST_ASSERT(
        mfs_superblock_v02_decode(
            fixture,
            sizeof(fixture),
            TEST_VOLUME_BYTES,
            &decoded
        ) == MFS_FORMAT_OK
    );

    MFS_TEST_ASSERT(
        decoded.total_blocks == TEST_VOLUME_BLOCKS
    );

    MFS_TEST_ASSERT(decoded.allocation_start == 2);
    MFS_TEST_ASSERT(decoded.allocation_blocks == 1);

    for (size_t i = 0; i < MFS_UUID_SIZE; i++)
        MFS_TEST_ASSERT(decoded.volume_uuid[i] == i);

    printf("PASS: v0.2 superblock golden decoding\n");
}

static void test_cross_version_rejection(void)
{
    uint8_t buffer[MFS_BLOCK_SIZE];

    struct mfs_superblock old_superblock = {
        .total_blocks = TEST_VOLUME_BLOCKS
    };

    struct mfs_superblock old_decoded = {0};
    struct mfs_superblock_v02 new_decoded = {0};

    for (size_t i = 0; i < MFS_UUID_SIZE; i++)
        old_superblock.volume_uuid[i] = (uint8_t)i;

    make_reference_fixture(buffer);

    MFS_TEST_ASSERT(
        mfs_superblock_decode(
            buffer,
            sizeof(buffer),
            TEST_VOLUME_BYTES,
            &old_decoded
        ) == MFS_FORMAT_UNSUPPORTED_VERSION
    );

    MFS_TEST_ASSERT(
        mfs_superblock_encode(
            buffer,
            sizeof(buffer),
            &old_superblock,
            TEST_VOLUME_BYTES
        ) == MFS_FORMAT_OK
    );

    MFS_TEST_ASSERT(
        mfs_superblock_v02_decode(
            buffer,
            sizeof(buffer),
            TEST_VOLUME_BYTES,
            &new_decoded
        ) == MFS_FORMAT_UNSUPPORTED_VERSION
    );

    printf("PASS: cross-version superblock rejection\n");
}

static void test_v02_decode_failures(void)
{
    uint8_t buffer[MFS_BLOCK_SIZE];

    make_reference_fixture(buffer);

    MFS_TEST_ASSERT(
        mfs_superblock_v02_decode(
            buffer,
            sizeof(buffer),
            TEST_VOLUME_BYTES,
            NULL
        ) == MFS_FORMAT_INVALID_ARGUMENT
    );

    check_rejected_decode(
        NULL,
        MFS_BLOCK_SIZE,
        TEST_VOLUME_BYTES,
        MFS_FORMAT_INVALID_ARGUMENT
    );

    check_rejected_decode(
        buffer,
        MFS_BLOCK_SIZE - 1,
        TEST_VOLUME_BYTES,
        MFS_FORMAT_INVALID_ARGUMENT
    );

    check_rejected_decode(
        buffer,
        MFS_BLOCK_SIZE,
        TEST_VOLUME_BYTES - MFS_BLOCK_SIZE,
        MFS_FORMAT_INVALID_DATA
    );

    /*
     * Corrupt only the stored checksum.
     */
    buffer[MFS_SUPERBLOCK_CRC_OFFSET] ^= 0x01;

    check_rejected_decode(
        buffer,
        MFS_BLOCK_SIZE,
        TEST_VOLUME_BYTES,
        MFS_FORMAT_INVALID_CHECKSUM
    );

    printf("PASS: v0.2 invalid decoding rejected\n");
    printf("PASS: v0.2 decode output preserved on failure\n");
}

static void test_v02_encode_failures(void)
{
    struct mfs_superblock_v02 valid;
    struct mfs_superblock_v02 invalid;

    make_reference_superblock(&valid);

    MFS_TEST_ASSERT(
        mfs_superblock_v02_encode(
            NULL,
            MFS_BLOCK_SIZE,
            &valid,
            TEST_VOLUME_BYTES
        ) == MFS_FORMAT_INVALID_ARGUMENT
    );

    check_rejected_encode(
        NULL,
        MFS_BLOCK_SIZE,
        TEST_VOLUME_BYTES,
        MFS_FORMAT_INVALID_ARGUMENT
    );

    check_rejected_encode(
        &valid,
        MFS_BLOCK_SIZE - 1,
        TEST_VOLUME_BYTES,
        MFS_FORMAT_INVALID_ARGUMENT
    );

    check_rejected_encode(
        &valid,
        0,
        TEST_VOLUME_BYTES,
        MFS_FORMAT_INVALID_ARGUMENT
    );

    /* Volume capacity must exactly match total_blocks. */
    check_rejected_encode(
        &valid,
        MFS_BLOCK_SIZE,
        TEST_VOLUME_BYTES - MFS_BLOCK_SIZE,
        MFS_FORMAT_INVALID_ARGUMENT
    );

    /* Allocation extent must begin at physical block 2. */
    invalid = valid;
    invalid.allocation_start = 3;

    check_rejected_encode(
        &invalid,
        MFS_BLOCK_SIZE,
        TEST_VOLUME_BYTES,
        MFS_FORMAT_INVALID_ARGUMENT
    );

    /* Bitmap length must match the derived geometry. */
    invalid = valid;
    invalid.allocation_blocks = 2;

    check_rejected_encode(
        &invalid,
        MFS_BLOCK_SIZE,
        TEST_VOLUME_BYTES,
        MFS_FORMAT_INVALID_ARGUMENT
    );

    /* Revision v0.2 requires at least four blocks. */
    invalid = valid;
    invalid.total_blocks = 3;

    check_rejected_encode(
        &invalid,
        MFS_BLOCK_SIZE,
        3 * MFS_BLOCK_SIZE,
        MFS_FORMAT_INVALID_ARGUMENT
    );

    /* Reject arithmetic overflow in volume geometry. */
    invalid = valid;
    invalid.total_blocks = UINT64_MAX;

    check_rejected_encode(
        &invalid,
        MFS_BLOCK_SIZE,
        UINT64_MAX,
        MFS_FORMAT_INVALID_ARGUMENT
    );

    /* An all-zero volume UUID is invalid. */
    invalid = valid;
    memset(invalid.volume_uuid, 0, MFS_UUID_SIZE);

    check_rejected_encode(
        &invalid,
        MFS_BLOCK_SIZE,
        TEST_VOLUME_BYTES,
        MFS_FORMAT_INVALID_ARGUMENT
    );

    printf("PASS: v0.2 invalid encoding rejected\n");
    printf("PASS: v0.2 encode output preserved on failure\n");
}

struct invalid_field_vector {
    size_t offset;
    uint8_t value;
    uint32_t checksum;
};

/*
 * CRC32C values were precomputed independently
 * for the complete 4096-byte mutated fixtures.
 *
 * The checksum field is treated as zero during
 * calculation, as required by the disk format.
 */
static void set_fixture_checksum(
    uint8_t *buffer,
    uint32_t checksum)
{
    buffer[MFS_SUPERBLOCK_CRC_OFFSET] =
        (uint8_t)checksum;

    buffer[MFS_SUPERBLOCK_CRC_OFFSET + 1] =
        (uint8_t)(checksum >> 8);

    buffer[MFS_SUPERBLOCK_CRC_OFFSET + 2] =
        (uint8_t)(checksum >> 16);

    buffer[MFS_SUPERBLOCK_CRC_OFFSET + 3] =
        (uint8_t)(checksum >> 24);
}

static void test_v02_semantic_failures(void)
{
    uint8_t buffer[MFS_BLOCK_SIZE];

    static const struct invalid_field_vector vectors[] = {
        {0,   0x4E, UINT32_C(0x59DE546A)},
        {12,  0x81, UINT32_C(0x94151CDA)},
        {14,  0x01, UINT32_C(0xF1C5289B)},
        {16,  0x01, UINT32_C(0xE2D1FA6A)},
        {20,  0x02, UINT32_C(0x93875EAF)},
        {48,  0x01, UINT32_C(0x9D193ADB)},
        {56,  0x01, UINT32_C(0xCB1A2FB0)},
        {64,  0x01, UINT32_C(0x87C118A7)},
        {72,  0x01, UINT32_C(0x3F3A06F6)},
        {80,  0x01, UINT32_C(0xDC6AC61C)},
        {88,  0x01, UINT32_C(0xC3EDE69E)},
        {96,  0x03, UINT32_C(0x2E3F74E2)},
        {104, 0x00, UINT32_C(0xD513353A)},
        {104, 0x02, UINT32_C(0x4D60D94D)},
        {116, 0x01, UINT32_C(0x32B64DE1)},
        {128, 0x01, UINT32_C(0xB16F8765)}
    };

    for (size_t i = 0;
         i < sizeof(vectors) / sizeof(vectors[0]);
         i++) {
        make_reference_fixture(buffer);

        buffer[vectors[i].offset] = vectors[i].value;

        set_fixture_checksum(
            buffer,
            vectors[i].checksum
        );

        check_rejected_decode(
            buffer,
            sizeof(buffer),
            TEST_VOLUME_BYTES,
            MFS_FORMAT_INVALID_DATA
        );
    }

    printf("PASS: v0.2 semantic inconsistencies rejected\n");
}

static void test_v02_zero_uuid_decode(void)
{
    uint8_t buffer[MFS_BLOCK_SIZE];

    make_reference_fixture(buffer);

    memset(buffer + 32, 0, MFS_UUID_SIZE);

    /*
     * CRC32C for the complete fixture with
     * an all-zero UUID.
     */
    set_fixture_checksum(
        buffer,
        UINT32_C(0xD2234C4C)
    );

    check_rejected_decode(
        buffer,
        sizeof(buffer),
        TEST_VOLUME_BYTES,
        MFS_FORMAT_INVALID_DATA
    );

    printf("PASS: v0.2 zero UUID rejected\n");
}

struct geometry_vector {
    uint64_t total_blocks;
    uint64_t allocation_blocks;
    uint32_t checksum;
};

/*
 * Encode a reference little-endian u64 independently
 * of the production superblock codec.
 */
static void write_reference_le64(
    uint8_t *buffer,
    uint64_t value)
{
    for (size_t i = 0; i < 8; i++) {
        buffer[i] = (uint8_t)value;
        value >>= 8;
    }
}

static void test_v02_geometry_boundaries(void)
{
    static const struct geometry_vector vectors[] = {
        {4, 1, UINT32_C(0x4D9E7392)},
        {32768, 1, UINT32_C(0xB76C8BCD)},
        {32769, 2, UINT32_C(0x2ED6DA9C)},
        {65536, 2, UINT32_C(0xBD5C3B60)},
        {65537, 3, UINT32_C(0xBC958646)},
        {
            UINT64_MAX / MFS_BLOCK_SIZE,
            UINT64_C(137438953472),
            UINT32_C(0xFD9F79F5)
        }
    };

    uint8_t fixture[MFS_BLOCK_SIZE];
    uint8_t encoded[MFS_BLOCK_SIZE];

    for (size_t i = 0;
         i < sizeof(vectors) / sizeof(vectors[0]);
         i++) {
        const uint64_t volume_bytes =
            vectors[i].total_blocks * MFS_BLOCK_SIZE;

        make_reference_fixture(fixture);

        write_reference_le64(
            fixture + 24,
            vectors[i].total_blocks
        );

        write_reference_le64(
            fixture + 104,
            vectors[i].allocation_blocks
        );

        set_fixture_checksum(
            fixture,
            vectors[i].checksum
        );

        struct mfs_superblock_v02 decoded = {0};

        MFS_TEST_ASSERT(
            mfs_superblock_v02_decode(
                fixture,
                sizeof(fixture),
                volume_bytes,
                &decoded
            ) == MFS_FORMAT_OK
        );

        MFS_TEST_ASSERT(
            decoded.total_blocks == vectors[i].total_blocks
        );

        MFS_TEST_ASSERT(decoded.allocation_start == 2);

        MFS_TEST_ASSERT(
            decoded.allocation_blocks ==
            vectors[i].allocation_blocks
        );

        for (size_t j = 0; j < MFS_UUID_SIZE; j++)
            MFS_TEST_ASSERT(decoded.volume_uuid[j] == j);

        struct mfs_superblock_v02 original;

        make_reference_superblock(&original);

        original.total_blocks = vectors[i].total_blocks;
        original.allocation_blocks =
            vectors[i].allocation_blocks;

        MFS_TEST_ASSERT(
            mfs_superblock_v02_encode(
                encoded,
                sizeof(encoded),
                &original,
                volume_bytes
            ) == MFS_FORMAT_OK
        );

        MFS_TEST_ASSERT(
            memcmp(encoded, fixture, sizeof(fixture)) == 0
        );
    }

    printf("PASS: v0.2 superblock geometry boundaries\n");
}

static void test_v02_below_minimum_volume(void)
{
    uint8_t buffer[MFS_BLOCK_SIZE];

    make_reference_fixture(buffer);

    write_reference_le64(buffer + 24, 3);

    set_fixture_checksum(
        buffer,
        UINT32_C(0x2B55CF4B)
    );

    check_rejected_decode(
        buffer,
        sizeof(buffer),
        3 * MFS_BLOCK_SIZE,
        MFS_FORMAT_INVALID_DATA
    );

    printf("PASS: v0.2 below-minimum volume rejected\n");
}

int main(void)
{
    test_v02_golden_encoding();
    test_v02_golden_decoding();
    test_cross_version_rejection();

    test_v02_decode_failures();
    test_v02_encode_failures();

    test_v02_semantic_failures();
    test_v02_zero_uuid_decode();

    test_v02_geometry_boundaries();
    test_v02_below_minimum_volume();

    printf("All v0.2 superblock reference tests passed.\n");
    return 0;
}
