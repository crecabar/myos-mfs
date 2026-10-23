// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (C) 2026 Cristian Recabarren
 */

#define _POSIX_C_SOURCE 200809L

#include <mfs/block_io.h>
#include <mfs/format.h>
#include <mfs/posix/block_device.h>

#include <fcntl.h>
#include <inttypes.h>
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

static void usage(const char *program)
{
    fprintf(stderr, "Usage: %s IMAGE\n", program);
}

static void print_superblock(const struct mfs_superblock *superblock)
{
    printf("MFS Volume Information\n");
    printf("----------------------\n");

    printf("Format version : %u.%u\n",
           MFS_FORMAT_MAJOR,
           MFS_FORMAT_MINOR);

    printf("Block size     : %u bytes\n", MFS_BLOCK_SIZE);

    printf("Total blocks   : %" PRIu64 "\n",
           superblock->total_blocks);

    printf("Volume size    : %" PRIu64 " bytes\n",
           superblock->total_blocks * MFS_BLOCK_SIZE);

    printf("Volume UUID    : ");

    for (size_t i = 0; i < MFS_UUID_SIZE; ++i) {
        printf("%02x", (unsigned int)superblock->volume_uuid[i]);

        if (i == 3 || i == 5 || i == 7 || i == 9)
            putchar('-');
    }

    putchar('\n');

    printf("Format state   : Experimental\n");
    printf("Superblock CRC : Valid\n");
}

static int inspect_image(const char *path)
{
    int fd = open(path, O_RDONLY);

    if (fd < 0) {
        perror("mfsdump: cannot open image");
        return 0;
    }

    struct mfs_posix_block_device adapter = {0};
    struct mfs_block_device device = {0};

    if (mfs_posix_block_device_init(&adapter, fd) != MFS_IO_OK) {
        fprintf(stderr, "mfsdump: invalid image geometry\n");
        goto fail;
    }

    if (mfs_posix_block_device_bind(&adapter, &device) != MFS_IO_OK) {
        fprintf(stderr, "mfsdump: cannot bind block device\n");
        goto fail;
    }

    uint8_t buffer[MFS_BLOCK_SIZE];

    if (mfs_block_read(&device, MFS_SUPERBLOCK_BLOCK,
                       buffer) != MFS_IO_OK) {
        fprintf(stderr, "mfsdump: cannot read superblock\n");
        goto fail;
    }

    /*
     * Decode and validate the persistent superblock.
     * The decoder checks the magic, version, geometry,
     * reserved fields, feature flags and CRC32C.
     */
    struct mfs_superblock superblock = {0};

    if (device.block_count > UINT64_MAX / MFS_BLOCK_SIZE) {
        fprintf(stderr, "mfsdump: volume size overflow\n");
        goto fail;
    }

    uint64_t volume_bytes = device.block_count * MFS_BLOCK_SIZE;

    if (mfs_superblock_decode(
        buffer,
        sizeof(buffer),
        volume_bytes,
        &superblock) != MFS_FORMAT_OK) {
        fprintf(stderr, "mfsdump: invalid MFS superblock\n");
        goto fail;
    }

    if (close(fd) != 0) {
        perror("mfsdump: cannot close image");
        return 0;
    }

    print_superblock(&superblock);

    return 1;

fail:
    if (close(fd) != 0)
        perror("mfsdump: cannot close image");

    return 0;
}

int main(int argc, char **argv)
{
    if (argc != 2 || argv[1][0] == '\0') {
        usage(argv[0]);
        return EXIT_FAILURE;
    }

    if (!inspect_image(argv[1]))
        return EXIT_FAILURE;

    return EXIT_SUCCESS;
}
