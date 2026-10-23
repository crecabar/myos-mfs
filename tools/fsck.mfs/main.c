// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (C) 2026 Cristian Recabarren
 */

#define _POSIX_C_SOURCE 200809L

#include <mfs/block_io.h>
#include <mfs/format.h>
#include <mfs/posix/block_device.h>

#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

enum fsck_exit_status {
    FSCK_EXIT_OK = 0,
    FSCK_EXIT_INCONSISTENT = 1,
    FSCK_EXIT_OPERATIONAL_ERROR = 2
};

static void usage(const char *program)
{
    fprintf(stderr, "Usage: %s IMAGE\n", program);
}

static enum fsck_exit_status check_reserved_block(
    struct mfs_block_device *device)
{
    uint8_t buffer[MFS_BLOCK_SIZE];

    if (mfs_block_read(device, 0, buffer) != MFS_IO_OK) {
        fprintf(stderr, "fsck.mfs: cannot read reserved block\n");
        return FSCK_EXIT_OPERATIONAL_ERROR;
    }

    for (size_t i = 0; i < sizeof(buffer); ++i) {
        if (buffer[i] != 0) {
            fprintf(stderr,
                    "fsck.mfs: reserved block is not zero-filled\n");

            return FSCK_EXIT_INCONSISTENT;
        }
    }

    return FSCK_EXIT_OK;
}

static enum fsck_exit_status check_image(const char *path)
{
    int fd = open(path, O_RDONLY);

    if (fd < 0) {
        perror("fsck.mfs: cannot open image");
        return FSCK_EXIT_OPERATIONAL_ERROR;
    }

    enum fsck_exit_status result = FSCK_EXIT_OPERATIONAL_ERROR;

    struct mfs_posix_block_device adapter = {0};
    struct mfs_block_device device = {0};

    if (mfs_posix_block_device_init(&adapter, fd) != MFS_IO_OK) {
        fprintf(stderr, "fsck.mfs: invalid image geometry\n");
        goto done;
    }

    if (mfs_posix_block_device_bind(&adapter, &device) != MFS_IO_OK) {
        fprintf(stderr, "fsck.mfs: cannot bind block device\n");
        goto done;
    }

    if (device.block_count > UINT64_MAX / MFS_BLOCK_SIZE) {
        fprintf(stderr, "fsck.mfs: volume size overflow\n");
        goto done;
    }

    uint8_t buffer[MFS_BLOCK_SIZE];

    if (mfs_block_read(&device, MFS_SUPERBLOCK_BLOCK,
                       buffer) != MFS_IO_OK) {
        fprintf(stderr, "fsck.mfs: cannot read superblock\n");
        goto done;
    }

    struct mfs_superblock superblock = {0};

    uint64_t volume_bytes = device.block_count * MFS_BLOCK_SIZE;

    if (mfs_superblock_decode(
        buffer,
        sizeof(buffer),
        volume_bytes,
        &superblock) != MFS_FORMAT_OK) {
        fprintf(stderr, "fsck.mfs: inconsistent superblock\n");
        result = FSCK_EXIT_INCONSISTENT;
        goto done;
    }

    result = check_reserved_block(&device);

    if (result != FSCK_EXIT_OK)
        goto done;

    result = FSCK_EXIT_OK;

done:
    if (close(fd) != 0) {
        perror("fsck.mfs: cannot close image");
        return FSCK_EXIT_OPERATIONAL_ERROR;
    }

    if (result == FSCK_EXIT_OK)
        printf("MFS volume: consistent (v0.1 superblock and reserved block)\n");

    return result;
}

int main(int argc, char **argv)
{
    if (argc != 2 || argv[1][0] == '\0') {
        usage(argv[0]);
        return FSCK_EXIT_OPERATIONAL_ERROR;
    }

    return check_image(argv[1]);
}
