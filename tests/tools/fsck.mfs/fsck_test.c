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
#include <mfs/format.h>

#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

enum {
    SUPERBLOCK_UUID_OFFSET = 32
};

struct fsck_test_resources {
    char directory[512];
    int directory_created;
};

static struct fsck_test_resources resources;

static void cleanup_fsck_test(void)
{
    if (!resources.directory_created)
        return;

    char image[512];

    int length = snprintf(
        image,
        sizeof(image),
        "%s/volume.mfs",
        resources.directory
    );

    if (length > 0 && (size_t)length < sizeof(image)) {
        if (unlink(image) != 0 && errno != ENOENT)
            perror("fsck_test: cleanup unlink");
    }

    if (rmdir(resources.directory) != 0 && errno != ENOENT)
        perror("fsck_test: cleanup rmdir");
}

static int run_mkfs(const char *mkfs,
                    const char *blocks,
                    const char *image)
{
    pid_t pid = fork();
    MFS_TEST_ASSERT(pid >= 0);

    if (pid == 0) {
        execl(mkfs, mkfs, "--blocks", blocks,
              image, (char *)NULL);
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

static int run_fsck(const char *fsck, const char *image)
{
    pid_t pid = fork();
    MFS_TEST_ASSERT(pid >= 0);

    if (pid == 0) {
        execl(fsck, fsck, image, (char *)NULL);
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

static void flip_image_byte(const char *path, off_t offset)
{
    int fd = open(path, O_RDWR);
    MFS_TEST_ASSERT(fd >= 0);

    uint8_t value;

    MFS_TEST_ASSERT(pread(fd, &value, sizeof(value), offset) ==
                    (ssize_t)sizeof(value));

    value ^= UINT8_C(1);

    MFS_TEST_ASSERT(pwrite(fd, &value, sizeof(value), offset) ==
                    (ssize_t)sizeof(value));

    MFS_TEST_ASSERT(close(fd) == 0);
}

static void read_image(const char *path,
                       uint8_t *buffer,
                       size_t size)
{
    int fd = open(path, O_RDONLY);
    MFS_TEST_ASSERT(fd >= 0);

    size_t completed = 0;

    while (completed < size) {
        ssize_t count = read(
            fd,
            buffer + completed,
            size - completed
        );

        if (count < 0) {
            if (errno == EINTR)
                continue;

            MFS_TEST_ASSERT(0);
        }

        MFS_TEST_ASSERT(count > 0);
        completed += (size_t)count;
    }

    MFS_TEST_ASSERT(close(fd) == 0);
}

static void test_valid_volume(const char *mkfs, const char *fsck)
{
    int length = snprintf(
        resources.directory,
        sizeof(resources.directory),
        "%s",
        "/tmp/fsck-mfs-test-XXXXXX"
    );

    MFS_TEST_ASSERT(length > 0);
    MFS_TEST_ASSERT((size_t)length < sizeof(resources.directory));

    MFS_TEST_ASSERT(mkdtemp(resources.directory) != NULL);
    resources.directory_created = 1;

    const char *directory = resources.directory;

    char image[512];

    length = snprintf(
        image,
        sizeof(image),
        "%s/volume.mfs",
        directory
    );

    MFS_TEST_ASSERT(length > 0);
    MFS_TEST_ASSERT((size_t)length < sizeof(image));

    MFS_TEST_ASSERT(run_mkfs(mkfs, "2", image) == 0);

    uint8_t before[2 * MFS_BLOCK_SIZE];
    uint8_t after[2 * MFS_BLOCK_SIZE];

    read_image(image, before, sizeof(before));

    MFS_TEST_ASSERT(run_fsck(fsck, image) == 0);

    read_image(image, after, sizeof(after));

    MFS_TEST_ASSERT(memcmp(before, after, sizeof(before)) == 0);

    printf("PASS: consistent volume preserved\n");

    printf("PASS: valid volume consistent\n");

    /*
     * Corrupt the UUID without updating the superblock CRC32C.
     */
    flip_image_byte(
        image,
        (off_t)MFS_SUPERBLOCK_BLOCK * MFS_BLOCK_SIZE
        + SUPERBLOCK_UUID_OFFSET
    );

    read_image(image, before, sizeof(before));

    MFS_TEST_ASSERT(run_fsck(fsck, image) == 1);

    read_image(image, after, sizeof(after));

    MFS_TEST_ASSERT(memcmp(before, after, sizeof(before)) == 0);

    printf("PASS: inconsistent superblock preserved\n");

    printf("PASS: corrupted superblock detected\n");

    /*
     * Restore the UUID and verify that consistency is recovered.
     */
    flip_image_byte(
        image,
        (off_t)MFS_SUPERBLOCK_BLOCK * MFS_BLOCK_SIZE
        + SUPERBLOCK_UUID_OFFSET
    );

    MFS_TEST_ASSERT(run_fsck(fsck, image) == 0);

    printf("PASS: restored superblock accepted\n");

    /*
     * Corrupt the reserved block without changing the superblock.
     */
    flip_image_byte(image, 0);

    read_image(image, before, sizeof(before));

    MFS_TEST_ASSERT(run_fsck(fsck, image) == 1);

    read_image(image, after, sizeof(after));

    MFS_TEST_ASSERT(memcmp(before, after, sizeof(before)) == 0);

    printf("PASS: inconsistent reserved block preserved\n");

    printf("PASS: corrupted reserved block detected\n");

    char missing_image[512];

    length = snprintf(
        missing_image,
        sizeof(missing_image),
        "%s/missing.mfs",
        directory
    );

    MFS_TEST_ASSERT(length > 0);
    MFS_TEST_ASSERT((size_t)length < sizeof(missing_image));

    MFS_TEST_ASSERT(access(missing_image, F_OK) != 0);
    MFS_TEST_ASSERT(errno == ENOENT);

    MFS_TEST_ASSERT(run_fsck(fsck, missing_image) == 2);

    printf("PASS: missing image reports operational error\n");
}

int main(int argc, char **argv)
{
    if (argc != 3) {
        fprintf(stderr,
                "Usage: %s MKFS_BINARY FSCK_BINARY\n", argv[0]);
        return EXIT_FAILURE;
    }

    MFS_TEST_ASSERT(atexit(cleanup_fsck_test) == 0);

    test_valid_volume(argv[1], argv[2]);

    printf("All fsck tests passed.\n");
    return EXIT_SUCCESS;
}
