// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (C) 2026 Cristian Recabarren
 */

#if defined(__APPLE__)
#define _DARWIN_C_SOURCE 1
#else
#define _POSIX_C_SOURCE 200809L
#endif

#include <mfs/assert.h>
#include <mfs/block_io.h>
#include <mfs/format.h>
#include <mfs/posix/block_device.h>

#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>


struct mkfs_test_resources {
    char directory[512];
    int directory_created;
};

static struct mkfs_test_resources resources;

static void cleanup_mkfs_test(void)
{
    if (resources.directory[0] == '\0')
        return;

    if (!resources.directory_created)
        return;

    const char *images[] = {
        "volume.mfs",
        "minimum.mfs",
        "invalid-0.mfs",
        "invalid-1.mfs",
        "invalid-2.mfs",
        "invalid-3.mfs",
        "invalid-4.mfs"
    };

    for (size_t i = 0;
         i < sizeof(images) / sizeof(images[0]);
         ++i) {
        char path[512];

        int length = snprintf(
            path,
            sizeof(path),
            "%s/%s",
            resources.directory,
            images[i]
        );

        if (length < 0 || (size_t)length >= sizeof(path))
            continue;

        if (unlink(path) != 0 && errno != ENOENT)
            perror("mkfs_test: cleanup unlink");
         }

    if (rmdir(resources.directory) != 0 && errno != ENOENT)
        perror("mkfs_test: cleanup rmdir");
}

static void test_formatted_volume(const char *path,
                                  uint64_t expected_blocks)
{
    int fd = open(path, O_RDONLY);
    MFS_TEST_ASSERT(fd >= 0);

    struct mfs_posix_block_device adapter = {0};
    struct mfs_block_device device = {0};

    MFS_TEST_ASSERT(mfs_posix_block_device_init(
        &adapter, fd) == MFS_IO_OK);

    MFS_TEST_ASSERT(mfs_posix_block_device_bind(
        &adapter, &device) == MFS_IO_OK);

    MFS_TEST_ASSERT(device.block_count == expected_blocks);

    /*
     * Block zero must remain reserved and zero-filled.
     */
    uint8_t buffer[MFS_BLOCK_SIZE];

    MFS_TEST_ASSERT(mfs_block_read(
        &device, 0, buffer) == MFS_IO_OK);

    for (size_t i = 0; i < MFS_BLOCK_SIZE; ++i)
        MFS_TEST_ASSERT(buffer[i] == 0);

    printf("PASS: reserved block zero\n");

    /*
     * Read and validate the persistent superblock.
     */
    MFS_TEST_ASSERT(mfs_block_read(
        &device, MFS_SUPERBLOCK_BLOCK,
        buffer) == MFS_IO_OK);

    struct mfs_superblock superblock = {0};

    MFS_TEST_ASSERT(mfs_superblock_decode(
        buffer,
        sizeof(buffer),
        expected_blocks * MFS_BLOCK_SIZE,
        &superblock) == MFS_FORMAT_OK);

    MFS_TEST_ASSERT(superblock.total_blocks == expected_blocks);

    uint8_t uuid_nonzero = 0;

    for (size_t i = 0; i < MFS_UUID_SIZE; ++i)
        uuid_nonzero |= superblock.volume_uuid[i];

    MFS_TEST_ASSERT(uuid_nonzero != 0);

    printf("PASS: persistent superblock decoded\n");

    MFS_TEST_ASSERT(close(fd) == 0);
}

static int run_mkfs(const char *mkfs_path,
                    const char *blocks,
                    const char *image_path)
{
    pid_t pid = fork();
    MFS_TEST_ASSERT(pid >= 0);

    if (pid == 0) {
        execl(
            mkfs_path,
            mkfs_path,
            "--blocks",
            blocks,
            image_path,
            (char *)NULL
        );

        _exit(127);
    }

    int status = 0;
    pid_t result;

    do {
        result = waitpid(pid, &status, 0);
    } while (result < 0 && errno == EINTR);

    MFS_TEST_ASSERT(result == pid);
    MFS_TEST_ASSERT(WIFEXITED(status));

    return WEXITSTATUS(status);
}


static void test_mkfs_integration(const char *mkfs_path)
{
    int length = snprintf(
        resources.directory,
        sizeof(resources.directory),
        "%s",
        "/tmp/mfs-mkfs-test-XXXXXX"
    );

    MFS_TEST_ASSERT(length > 0);
    MFS_TEST_ASSERT((size_t)length < sizeof(resources.directory));

    MFS_TEST_ASSERT(mkdtemp(resources.directory) != NULL);
    resources.directory_created = 1;

    const char *directory = resources.directory;

    MFS_TEST_ASSERT(length > 0);
    MFS_TEST_ASSERT((size_t)length < sizeof(resources.directory));

    char image_path[512];

    length = snprintf(
        image_path,
        sizeof(image_path),
        "%s/volume.mfs",
        directory
    );

    MFS_TEST_ASSERT(length > 0);
    MFS_TEST_ASSERT((size_t)length < sizeof(image_path));

    MFS_TEST_ASSERT(run_mkfs(
        mkfs_path,
        "16384",
        image_path) == 0);

    test_formatted_volume(image_path, UINT64_C(16384));

    /*
     * Formatting an existing image must fail without modifying it.
     */
    struct stat before = {0};
    struct stat after = {0};

    MFS_TEST_ASSERT(stat(image_path, &before) == 0);

    uint8_t original[MFS_BLOCK_SIZE];

    int fd = open(image_path, O_RDONLY);
    MFS_TEST_ASSERT(fd >= 0);

    MFS_TEST_ASSERT(
        pread(fd, original, sizeof(original),
              (off_t)MFS_BLOCK_SIZE) == (ssize_t)sizeof(original));

    MFS_TEST_ASSERT(close(fd) == 0);

    /*
     * A second invocation must reject the existing pathname.
     */
    MFS_TEST_ASSERT(run_mkfs(
        mkfs_path,
        "16384",
        image_path) != 0);

    MFS_TEST_ASSERT(stat(image_path, &after) == 0);
    MFS_TEST_ASSERT(before.st_size == after.st_size);

    uint8_t current[MFS_BLOCK_SIZE];

    fd = open(image_path, O_RDONLY);
    MFS_TEST_ASSERT(fd >= 0);

    MFS_TEST_ASSERT(
        pread(fd, current, sizeof(current),
              (off_t)MFS_BLOCK_SIZE) == (ssize_t)sizeof(current));

    MFS_TEST_ASSERT(close(fd) == 0);

    for (size_t i = 0; i < MFS_BLOCK_SIZE; ++i)
        MFS_TEST_ASSERT(original[i] == current[i]);

    printf("PASS: existing image preserved\n");

    /*
     * The minimum valid MFS volume contains two blocks:
     * the reserved block and the superblock.
     */
    char minimum_path[512];

    length = snprintf(
        minimum_path,
        sizeof(minimum_path),
        "%s/minimum.mfs",
        directory
    );

    MFS_TEST_ASSERT(length > 0);
    MFS_TEST_ASSERT((size_t)length < sizeof(minimum_path));

    MFS_TEST_ASSERT(run_mkfs(
        mkfs_path,
        "2",
        minimum_path) == 0);

    test_formatted_volume(minimum_path, UINT64_C(2));

    printf("PASS: minimum volume created\n");

    /*
     * Invalid geometry must not create an output file.
     */
    const char *invalid_blocks[] = {
        "0",
        "1",
        "abc",
        "+16384",
        "18446744073709551615"
    };

    for (size_t i = 0;
         i < sizeof(invalid_blocks) / sizeof(invalid_blocks[0]);
         ++i)
    {
        char invalid_path[512];

        length = snprintf(
            invalid_path,
            sizeof(invalid_path),
            "%s/invalid-%zu.mfs",
            directory,
            i
        );

        MFS_TEST_ASSERT(length > 0);
        MFS_TEST_ASSERT((size_t)length < sizeof(invalid_path));

        MFS_TEST_ASSERT(run_mkfs(
            mkfs_path,
            invalid_blocks[i],
            invalid_path) != 0);

        struct stat st;

        MFS_TEST_ASSERT(
            lstat(invalid_path, &st) != 0 && errno == ENOENT);
    }

    printf("PASS: invalid geometry rejected\n");

    printf("PASS: mkfs integration\n");
}

int main(int argc, char **argv)
{
    if (argc != 2) {
        fprintf(stderr, "Usage: %s MKFS_BINARY\n", argv[0]);
        return EXIT_FAILURE;
    }

    MFS_TEST_ASSERT(atexit(cleanup_mkfs_test) == 0);
    test_mkfs_integration(argv[1]);

    printf("All MFS image tests passed.\n");

    return EXIT_SUCCESS;
}
