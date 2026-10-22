
// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (C) 2026 Cristian Recabarren
 */

#define _POSIX_C_SOURCE 200809L

#include <mfs/posix/block_device.h>
#include <mfs/assert.h>

#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#define TEST_BLOCK_COUNT 4U
#define TEST_VOLUME_BYTES (TEST_BLOCK_COUNT * MFS_BLOCK_SIZE)

static void test_read_from_regular_file(void)
{
    char path[] = "/tmp/mfs-block-io-XXXXXX";

    int fd = mkstemp(path);
    MFS_TEST_ASSERT(fd >= 0);

    /*
     * Remove the directory entry immediately.
     * The file remains accessible through fd.
     */
    MFS_TEST_ASSERT(unlink(path) == 0);

    MFS_TEST_ASSERT(ftruncate(
        fd, TEST_VOLUME_BYTES) == 0);

    uint8_t expected[MFS_BLOCK_SIZE];

    for (size_t i = 0; i < MFS_BLOCK_SIZE; ++i)
        expected[i] = (uint8_t)(i & 0xFFU);

    /*
     * Write known bytes to block 1 using POSIX.
     */
    ssize_t written = pwrite(
        fd,
        expected,
        sizeof(expected),
        (off_t)MFS_BLOCK_SIZE
    );

    MFS_TEST_ASSERT(written == (ssize_t)sizeof(expected));

    /*
     * Initialize and bind the POSIX adapter.
     */
    struct mfs_posix_block_device adapter = {0};
    struct mfs_block_device device = {0};

    MFS_TEST_ASSERT(mfs_posix_block_device_init(
        &adapter, fd) == MFS_IO_OK);

    MFS_TEST_ASSERT(mfs_posix_block_device_bind(
        &adapter, &device) == MFS_IO_OK);

    MFS_TEST_ASSERT(device.block_count == TEST_BLOCK_COUNT);

    /*
     * Read the same block through portable MFS I/O.
     */
    uint8_t actual[MFS_BLOCK_SIZE] = {0};

    MFS_TEST_ASSERT(mfs_block_read(
        &device, MFS_SUPERBLOCK_BLOCK,
        actual) == MFS_IO_OK);

    for (size_t i = 0; i < MFS_BLOCK_SIZE; ++i)
        MFS_TEST_ASSERT(actual[i] == expected[i]);

    MFS_TEST_ASSERT(close(fd) == 0);

    printf("PASS: POSIX regular file block read\n");
}

static void test_write_read_flush(void)
{
    char path[] = "/tmp/mfs-block-write-XXXXXX";

    int fd = mkstemp(path);
    MFS_TEST_ASSERT(fd >= 0);
    MFS_TEST_ASSERT(unlink(path) == 0);
    MFS_TEST_ASSERT(ftruncate(fd, TEST_VOLUME_BYTES) == 0);

    struct mfs_posix_block_device adapter = {0};
    struct mfs_block_device device = {0};

    MFS_TEST_ASSERT(mfs_posix_block_device_init(
        &adapter, fd) == MFS_IO_OK);

    MFS_TEST_ASSERT(mfs_posix_block_device_bind(
        &adapter, &device) == MFS_IO_OK);

    uint8_t expected[MFS_BLOCK_SIZE];
    uint8_t actual[MFS_BLOCK_SIZE] = {0};

    for (size_t i = 0; i < MFS_BLOCK_SIZE; ++i)
        expected[i] = (uint8_t)((i * 7U) & 0xFFU);

    MFS_TEST_ASSERT(mfs_block_write(
        &device, 2, expected) == MFS_IO_OK);

    MFS_TEST_ASSERT(mfs_block_flush(
        &device) == MFS_IO_OK);

    MFS_TEST_ASSERT(mfs_block_read(
        &device, 2, actual) == MFS_IO_OK);

    for (size_t i = 0; i < MFS_BLOCK_SIZE; ++i)
        MFS_TEST_ASSERT(actual[i] == expected[i]);

    MFS_TEST_ASSERT(close(fd) == 0);

    printf("PASS: POSIX block write/read/flush\n");
}

static void test_invalid_geometry(void)
{
    char path[] = "/tmp/mfs-invalid-geometry-XXXXXX";

    int fd = mkstemp(path);
    MFS_TEST_ASSERT(fd >= 0);
    MFS_TEST_ASSERT(unlink(path) == 0);

    struct mfs_posix_block_device adapter = {0};

    /*
     * Empty files cannot represent MFS volumes.
     */
    MFS_TEST_ASSERT(mfs_posix_block_device_init(
        &adapter, fd) == MFS_IO_INVALID_ARGUMENT);

    /*
     * A volume smaller than two blocks is invalid.
     */
    MFS_TEST_ASSERT(ftruncate(fd, MFS_BLOCK_SIZE) == 0);

    MFS_TEST_ASSERT(mfs_posix_block_device_init(
        &adapter, fd) == MFS_IO_INVALID_ARGUMENT);

    /*
     * The size must be an exact multiple of 4096.
     */
    MFS_TEST_ASSERT(ftruncate(
        fd, 2 * MFS_BLOCK_SIZE + 1) == 0);

    MFS_TEST_ASSERT(mfs_posix_block_device_init(
        &adapter, fd) == MFS_IO_INVALID_ARGUMENT);

    MFS_TEST_ASSERT(close(fd) == 0);

    printf("PASS: POSIX invalid geometry\n");
}

static void test_truncated_volume_read(void)
{
    char path[] = "/tmp/mfs-truncated-XXXXXX";

    int fd = mkstemp(path);
    MFS_TEST_ASSERT(fd >= 0);
    MFS_TEST_ASSERT(unlink(path) == 0);

    MFS_TEST_ASSERT(ftruncate(fd, TEST_VOLUME_BYTES) == 0);

    struct mfs_posix_block_device adapter = {0};
    struct mfs_block_device device = {0};

    MFS_TEST_ASSERT(mfs_posix_block_device_init(
        &adapter, fd) == MFS_IO_OK);

    MFS_TEST_ASSERT(mfs_posix_block_device_bind(
        &adapter, &device) == MFS_IO_OK);

    /*
     * Remove the last two blocks after initialization.
     */
    MFS_TEST_ASSERT(ftruncate(
        fd, 2 * MFS_BLOCK_SIZE) == 0);

    uint8_t buffer[MFS_BLOCK_SIZE] = {0};

    MFS_TEST_ASSERT(mfs_block_read(
        &device, 3, buffer) == MFS_IO_ERROR);

    MFS_TEST_ASSERT(close(fd) == 0);

    printf("PASS: POSIX truncated volume read\n");
}

static void test_read_only_descriptor(void)
{
    char path[] = "/tmp/mfs-read-only-XXXXXX";

    int fd = mkstemp(path);
    MFS_TEST_ASSERT(fd >= 0);

    MFS_TEST_ASSERT(ftruncate(fd, TEST_VOLUME_BYTES) == 0);
    MFS_TEST_ASSERT(close(fd) == 0);

    fd = open(path, O_RDONLY);
    MFS_TEST_ASSERT(fd >= 0);

    MFS_TEST_ASSERT(unlink(path) == 0);

    struct mfs_posix_block_device adapter = {0};
    struct mfs_block_device device = {0};

    MFS_TEST_ASSERT(mfs_posix_block_device_init(
        &adapter, fd) == MFS_IO_OK);

    MFS_TEST_ASSERT(mfs_posix_block_device_bind(
        &adapter, &device) == MFS_IO_OK);

    uint8_t buffer[MFS_BLOCK_SIZE] = {0};

    MFS_TEST_ASSERT(mfs_block_read(
        &device, 0, buffer) == MFS_IO_OK);

    MFS_TEST_ASSERT(mfs_block_write(
        &device, 0, buffer) == MFS_IO_ERROR);

    MFS_TEST_ASSERT(close(fd) == 0);

    printf("PASS: POSIX read-only descriptor\n");
}

static void test_closed_descriptor(void)
{
    char path[] = "/tmp/mfs-closed-fd-XXXXXX";

    int fd = mkstemp(path);
    MFS_TEST_ASSERT(fd >= 0);
    MFS_TEST_ASSERT(unlink(path) == 0);
    MFS_TEST_ASSERT(ftruncate(fd, TEST_VOLUME_BYTES) == 0);

    struct mfs_posix_block_device adapter = {0};
    struct mfs_block_device device = {0};

    MFS_TEST_ASSERT(mfs_posix_block_device_init(
        &adapter, fd) == MFS_IO_OK);

    MFS_TEST_ASSERT(mfs_posix_block_device_bind(
        &adapter, &device) == MFS_IO_OK);

    MFS_TEST_ASSERT(close(fd) == 0);

    uint8_t buffer[MFS_BLOCK_SIZE] = {0};

    MFS_TEST_ASSERT(mfs_block_read(
        &device, 0, buffer) == MFS_IO_ERROR);

    MFS_TEST_ASSERT(mfs_block_write(
        &device, 0, buffer) == MFS_IO_ERROR);

    MFS_TEST_ASSERT(mfs_block_flush(
        &device) == MFS_IO_ERROR);

    printf("PASS: POSIX closed descriptor\n");
}

static void test_invalid_descriptor(void)
{
    struct mfs_posix_block_device adapter = {0};

    MFS_TEST_ASSERT(mfs_posix_block_device_init(
        NULL, -1) == MFS_IO_INVALID_ARGUMENT);

    MFS_TEST_ASSERT(mfs_posix_block_device_init(
        &adapter, -1) == MFS_IO_INVALID_ARGUMENT);

    printf("PASS: POSIX invalid descriptor\n");
}

static void test_invalid_bind(void)
{
    struct mfs_posix_block_device adapter = {
        .fd = -1,
        .block_count = 0
    };

    struct mfs_block_device device = {0};

    MFS_TEST_ASSERT(mfs_posix_block_device_bind(
        NULL, &device) == MFS_IO_INVALID_ARGUMENT);

    MFS_TEST_ASSERT(mfs_posix_block_device_bind(
        &adapter, NULL) == MFS_IO_INVALID_ARGUMENT);

    MFS_TEST_ASSERT(mfs_posix_block_device_bind(
        &adapter, &device) == MFS_IO_INVALID_ARGUMENT);

    printf("PASS: POSIX invalid bind\n");
}

int main(void)
{
    test_read_from_regular_file();
    test_write_read_flush();

    test_invalid_geometry();
    test_truncated_volume_read();
    test_read_only_descriptor();

    test_closed_descriptor();
    test_invalid_descriptor();
    test_invalid_bind();

    printf("All POSIX block device tests passed.\n");

    return 0;
}
