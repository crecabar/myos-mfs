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

struct mfsdump_test_resources {
    char directory[512];
    int directory_created;
};

static struct mfsdump_test_resources resources;

static void cleanup_mfsdump_test(void)
{
    if (!resources.directory_created)
        return;

    const char *images[] = {
        "volume.mfs",
        "corrupt.mfs",
        "read-only.mfs"
    };

    for (size_t i = 0;
         i < sizeof(images) / sizeof(images[0]);
         ++i)
    {
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
            perror("mfsdump_test: cleanup unlink");
    }

    if (rmdir(resources.directory) != 0 && errno != ENOENT)
        perror("mfsdump_test: cleanup rmdir");
}

static void corrupt_superblock_uuid(const char *path)
{
    int fd = open(path, O_RDWR);
    MFS_TEST_ASSERT(fd >= 0);

    /*
     * Volume UUID starts at offset 32 within the superblock.
     */
    enum {
        SUPERBLOCK_UUID_OFFSET = 32
    };

    off_t offset = (off_t)MFS_SUPERBLOCK_BLOCK * MFS_BLOCK_SIZE
                 + SUPERBLOCK_UUID_OFFSET;

    unsigned char value;

    MFS_TEST_ASSERT(pread(fd, &value, 1, offset) == 1);

    /*
     * Flip one bit, guaranteeing that the byte changes.
     */
    value ^= 0x01;

    MFS_TEST_ASSERT(pwrite(fd, &value, 1, offset) == 1);

    MFS_TEST_ASSERT(close(fd) == 0);
}

static int run_tool_capture(const char *tool,
                            const char *argument,
                            char *output,
                            size_t capacity)
{
    int pipefd[2];

    MFS_TEST_ASSERT(output != NULL);
    MFS_TEST_ASSERT(capacity > 0);
    MFS_TEST_ASSERT(pipe(pipefd) == 0);

    pid_t pid = fork();
    MFS_TEST_ASSERT(pid >= 0);

    if (pid == 0) {
        close(pipefd[0]);

        if (dup2(pipefd[1], STDOUT_FILENO) < 0)
            _exit(127);

        close(pipefd[1]);

        execl(tool, tool, argument, (char *)NULL);
        _exit(127);
    }

    MFS_TEST_ASSERT(close(pipefd[1]) == 0);

    size_t used = 0;
    int truncated = 0;
    char chunk[512];

    for (;;) {
        ssize_t count = read(pipefd[0], chunk, sizeof(chunk));

        if (count < 0) {
            if (errno == EINTR)
                continue;

            MFS_TEST_ASSERT(0);
        }

        if (count == 0)
            break;

        size_t available = capacity - 1 - used;
        size_t received = (size_t)count;
        size_t copied = received < available ? received : available;

        if (copied > 0) {
            memcpy(output + used, chunk, copied);
            used += copied;
        }

        if (copied < received)
            truncated = 1;
    }

    output[used] = '\0';

    MFS_TEST_ASSERT(close(pipefd[0]) == 0);

    int status = 0;
    pid_t result;

    do {
        result = waitpid(pid, &status, 0);
    } while (result < 0 && errno == EINTR);

    MFS_TEST_ASSERT(result == pid);
    MFS_TEST_ASSERT(WIFEXITED(status));

    MFS_TEST_ASSERT(!truncated);

    return WEXITSTATUS(status);
}

static int run_mkfs(const char *mkfs,
                    const char *blocks,
                    const char *image)
{
    pid_t pid = fork();
    MFS_TEST_ASSERT(pid >= 0);

    if (pid == 0) {
        execl(
            mkfs,
            mkfs,
            "--blocks",
            blocks,
            image,
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

static void test_read_only_inspection(const char *mkfs,
                                      const char *mfsdump,
                                      const char *directory)
{
    char image[512];

    int length = snprintf(
        image,
        sizeof(image),
        "%s/read-only.mfs",
        directory
    );

    MFS_TEST_ASSERT(length > 0);
    MFS_TEST_ASSERT((size_t)length < sizeof(image));

    MFS_TEST_ASSERT(run_mkfs(mkfs, "2", image) == 0);

    uint8_t before[2 * MFS_BLOCK_SIZE];
    uint8_t after[2 * MFS_BLOCK_SIZE];

    read_image(image, before, sizeof(before));

    char output[4096];

    MFS_TEST_ASSERT(run_tool_capture(
        mfsdump,
        image,
        output,
        sizeof(output)) == 0);

    read_image(image, after, sizeof(after));

    MFS_TEST_ASSERT(memcmp(before, after, sizeof(before)) == 0);

    printf("PASS: inspection preserves volume bytes\n");
}

static void test_valid_volume(const char *mkfs,
                              const char *mfsdump)
{
    int length = snprintf(
        resources.directory,
        sizeof(resources.directory),
        "%s",
        "/tmp/mfsdump-test-XXXXXX"
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

    MFS_TEST_ASSERT(run_mkfs(
        mkfs,
        "16384",
        image) == 0);

    char output[4096];

    MFS_TEST_ASSERT(run_tool_capture(
        mfsdump,
        image,
        output,
        sizeof(output)) == 0);

    MFS_TEST_ASSERT(strstr(
        output, "MFS Volume Information") != NULL);

    MFS_TEST_ASSERT(strstr(
        output, "Format version : 0.1") != NULL);

    MFS_TEST_ASSERT(strstr(
        output, "Block size     : 4096 bytes") != NULL);

    MFS_TEST_ASSERT(strstr(
        output, "Total blocks   : 16384") != NULL);

    MFS_TEST_ASSERT(strstr(
        output, "Volume size    : 67108864 bytes") != NULL);

    MFS_TEST_ASSERT(strstr(
        output, "Format state   : Experimental") != NULL);

    MFS_TEST_ASSERT(strstr(
        output, "Superblock CRC : Valid") != NULL);

    printf("PASS: mfsdump metadata output\n");

    printf("PASS: valid MFS volume recognized\n");

    char corrupt_image[512];

    length = snprintf(
        corrupt_image,
        sizeof(corrupt_image),
        "%s/corrupt.mfs",
        directory
    );

    MFS_TEST_ASSERT(length > 0);
    MFS_TEST_ASSERT((size_t)length < sizeof(corrupt_image));

    MFS_TEST_ASSERT(run_mkfs(
        mkfs,
        "2",
        corrupt_image) == 0);

    corrupt_superblock_uuid(corrupt_image);

    uint8_t corrupt_before[2 * MFS_BLOCK_SIZE];
    uint8_t corrupt_after[2 * MFS_BLOCK_SIZE];

    read_image(corrupt_image,
               corrupt_before,
               sizeof(corrupt_before));

    char corrupt_output[4096];

    MFS_TEST_ASSERT(run_tool_capture(
        mfsdump,
        corrupt_image,
        corrupt_output,
        sizeof(corrupt_output)) != 0);

    MFS_TEST_ASSERT(corrupt_output[0] == '\0');

    printf("PASS: corrupted superblock rejected\n");

    read_image(corrupt_image,
           corrupt_after,
           sizeof(corrupt_after));

    MFS_TEST_ASSERT(memcmp(
        corrupt_before,
        corrupt_after,
        sizeof(corrupt_before)) == 0);

    printf("PASS: corrupted volume preserved\n");

    test_read_only_inspection(mkfs, mfsdump, directory);
}

int main(int argc, char **argv)
{
    if (argc != 3) {
        fprintf(stderr,
                "Usage: %s MKFS_BINARY MFSDUMP_BINARY\n",
                argv[0]);
        return EXIT_FAILURE;
    }

    MFS_TEST_ASSERT(atexit(cleanup_mfsdump_test) == 0);

    test_valid_volume(argv[1], argv[2]);

    printf("All mfsdump tests passed.\n");

    return EXIT_SUCCESS;
}
