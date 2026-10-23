// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (C) 2026 Cristian Recabarren
 */

#define _POSIX_C_SOURCE 200809L

#include <mfs/format.h>
#include <mfs/block_io.h>
#include <mfs/posix/block_device.h>

#include <errno.h>
#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

_Static_assert(sizeof(off_t) == sizeof(int64_t),
               "mkfs.mfs requires a 64-bit off_t");

_Static_assert((off_t)-1 < (off_t)0,
               "mkfs.mfs requires a signed off_t");

struct mkfs_options {
    const char *path;
    uint64_t blocks;
    uint64_t volume_bytes;
};

static void usage(const char *program)
{
    fprintf(stderr,
            "Usage: %s --blocks BLOCKS IMAGE\n",
            program);
}

static int parse_blocks(const char *text, uint64_t *blocks)
{
    if (text == NULL || blocks == NULL)
        return 0;

    /*
     * Accept only a nonempty string of decimal digits.
     * This excludes signs, whitespace and suffixes.
     */
    if (*text == '\0')
        return 0;

    for (const char *p = text; *p != '\0'; ++p) {
        if (*p < '0' || *p > '9')
            return 0;
    }

    errno = 0;
    char *end = NULL;
    uintmax_t value = strtoumax(text, &end, 10);

    if (errno == ERANGE || end == text || *end != '\0')
        return 0;

    if (value < 2 || value > UINT64_MAX / MFS_BLOCK_SIZE)
        return 0;

    *blocks = (uint64_t)value;
    return 1;
}

static int parse_options(int argc, char **argv,
                         struct mkfs_options *options)
{
    if (options == NULL)
        return 0;

    if (argc != 4)
        return 0;

    if (argv[1] == NULL || argv[2] == NULL ||
        argv[3] == NULL)
        return 0;

    if (strcmp(argv[1], "--blocks") != 0)
        return 0;

    if (argv[3][0] == '\0')
        return 0;

    uint64_t blocks;

    if (!parse_blocks(argv[2], &blocks))
        return 0;

    options->path = argv[3];
    options->blocks = blocks;
    options->volume_bytes = blocks * MFS_BLOCK_SIZE;

    return 1;
}

static int generate_volume_uuid(uint8_t uuid[MFS_UUID_SIZE])
{
    int fd = open("/dev/urandom", O_RDONLY);

    if (fd < 0)
        return 0;

    size_t completed = 0;

    while (completed < MFS_UUID_SIZE) {
        ssize_t count = read(
            fd,
            uuid + completed,
            MFS_UUID_SIZE - completed
        );

        if (count < 0) {
            if (errno == EINTR)
                continue;

            close(fd);
            return 0;
        }

        if (count == 0) {
            close(fd);
            return 0;
        }

        completed += (size_t)count;
    }

    if (close(fd) != 0)
        return 0;

    /*
     * A zero UUID is reserved as invalid by MFS.
     */
    uint8_t nonzero = 0;

    for (size_t i = 0; i < MFS_UUID_SIZE; ++i)
        nonzero |= uuid[i];

    return nonzero != 0;
}

static int write_superblock(int fd,
                            const struct mkfs_options *options)
{
    struct mfs_posix_block_device adapter = {0};
    struct mfs_block_device device = {0};

    if (mfs_posix_block_device_init(&adapter, fd) != MFS_IO_OK)
        return 0;

    if (mfs_posix_block_device_bind(&adapter, &device) != MFS_IO_OK)
        return 0;

    struct mfs_superblock superblock = {
        .total_blocks = options->blocks
    };

    if (!generate_volume_uuid(superblock.volume_uuid))
        return 0;

    uint8_t buffer[MFS_BLOCK_SIZE];

    if (mfs_superblock_encode(
        buffer,
        sizeof(buffer),
        &superblock,
        options->volume_bytes) != MFS_FORMAT_OK)
        return 0;

    if (mfs_block_write(
        &device,
        MFS_SUPERBLOCK_BLOCK,
        buffer) != MFS_IO_OK)
        return 0;

    return 1;
}

static int create_image(const struct mkfs_options *options)
{
    /*
     * The volume must fit into the host's off_t.
     *
     * This first implementation supports POSIX hosts
     * with a signed 64-bit off_t.
     */
    if (options->volume_bytes > INT64_MAX) {
        fprintf(stderr, "mkfs.mfs: volume too large\n");
        return 0;
    }

    /*
     * O_EXCL prevents overwriting an existing path.
     * O_NOFOLLOW, when available, provides an additional
     * defense against symbolic links.
     */
    int flags = O_WRONLY | O_CREAT | O_EXCL;

#ifdef O_NOFOLLOW
    flags |= O_NOFOLLOW;
#endif

    int fd = open(options->path, flags, 0600);

    if (fd < 0) {
        perror("mkfs.mfs: cannot create image");
        return 0;
    }

    /*
     * From here onwards, this invocation owns the
     * newly created file.
     */
    if (ftruncate(fd, (off_t)options->volume_bytes) != 0) {
        perror("mkfs.mfs: cannot size image");
        goto fail;
    }

    if (!write_superblock(fd, options)) {
        fprintf(stderr, "mkfs.mfs: cannot write superblock\n");
        goto fail;
    }

    if (fsync(fd) != 0) {
        perror("mkfs.mfs: cannot synchronize image");
        goto fail;
    }

    if (close(fd) != 0) {
        perror("mkfs.mfs: cannot close image");
        return 0;
    }

    return 1;

fail:
    if (close(fd) != 0)
        perror("mkfs.mfs: cannot close incomplete image");

    fprintf(stderr,
            "mkfs.mfs: incomplete image retained at %s\n",
            options->path);

    return 0;
}

int main(int argc, char **argv)
{
    struct mkfs_options options = {0};

    if (!parse_options(argc, argv, &options)) {
        usage(argv[0]);
        return EXIT_FAILURE;
    }

    printf("MFS experimental format v%u.%u\n",
           MFS_FORMAT_MAJOR, MFS_FORMAT_MINOR);

    printf("Image: %s\n", options.path);

    printf("Blocks: %" PRIu64 "\n", options.blocks);

    printf("Volume bytes: %" PRIu64 "\n",
           options.volume_bytes);

    if (!create_image(&options))
        return EXIT_FAILURE;

    printf("Image created successfully.\n");

    return EXIT_SUCCESS;
}
