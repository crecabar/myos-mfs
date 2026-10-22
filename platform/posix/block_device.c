// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (C) 2026 Cristian Recabarren
 */

#define _POSIX_C_SOURCE 200809L

#include <mfs/posix/block_device.h>

#include <sys/stat.h>
#include <sys/types.h>

#include <stdint.h>
#include <errno.h>
#include <limits.h>
#include <stddef.h>
#include <unistd.h>

enum mfs_io_result
mfs_posix_block_device_init(struct mfs_posix_block_device *adapter,
                           int fd)
{
    if (adapter == NULL || fd < 0)
        return MFS_IO_INVALID_ARGUMENT;

    struct stat st;

    if (fstat(fd, &st) != 0)
        return MFS_IO_ERROR;

    if (!S_ISREG(st.st_mode))
        return MFS_IO_INVALID_ARGUMENT;

    if (st.st_size < 0)
        return MFS_IO_INVALID_ARGUMENT;

    uint64_t size = (uint64_t)st.st_size;

    if (size % MFS_BLOCK_SIZE != 0)
        return MFS_IO_INVALID_ARGUMENT;

    uint64_t block_count = size / MFS_BLOCK_SIZE;

    if (block_count < 2)
        return MFS_IO_INVALID_ARGUMENT;

    adapter->fd = fd;
    adapter->block_count = block_count;

    return MFS_IO_OK;
}

/*
 * Convert an MFS block index into a POSIX file offset.
 *
 * The caller guarantees that the block belongs to the
 * initialized volume.
 */
static int
block_offset(uint64_t block, off_t *offset)
{
    if (offset == NULL)
        return 0;

    /*
     * off_t must represent the complete 4096-byte block.
     * This backend requires a signed 64-bit or narrower
     * POSIX file offset type.
     */
    if (sizeof(off_t) > sizeof(uint64_t))
        return 0;

    uint64_t off_max;

    if (sizeof(off_t) == sizeof(uint64_t)) {
        off_max = INT64_MAX;
    } else {
        unsigned bits = (unsigned)(sizeof(off_t) * CHAR_BIT);
        off_max = (UINT64_C(1) << (bits - 1U)) - 1U;
    }

    if (block > off_max / MFS_BLOCK_SIZE)
        return 0;

    uint64_t start = block * MFS_BLOCK_SIZE;

    if (start > off_max - (MFS_BLOCK_SIZE - 1U))
        return 0;

    *offset = (off_t)start;

    return 1;
}


static enum mfs_io_result
posix_read(void *context, uint64_t block, uint8_t *buffer)
{
    struct mfs_posix_block_device *adapter = context;
    off_t start;

    if (!block_offset(block, &start))
        return MFS_IO_OUT_OF_RANGE;

    size_t completed = 0;

    while (completed < MFS_BLOCK_SIZE) {
        off_t offset = start + (off_t)completed;

        ssize_t transferred = pread(
            adapter->fd,
            buffer + completed,
            MFS_BLOCK_SIZE - completed,
            offset
        );

        if (transferred < 0) {
            if (errno == EINTR)
                continue;

            return MFS_IO_ERROR;
        }

        if (transferred == 0)
            return MFS_IO_ERROR;

        completed += (size_t)transferred;
    }

    return MFS_IO_OK;
}

static enum mfs_io_result
posix_write(void *context, uint64_t block,
            const uint8_t *buffer)
{
    struct mfs_posix_block_device *adapter = context;
    off_t start;

    if (!block_offset(block, &start))
        return MFS_IO_OUT_OF_RANGE;

    size_t completed = 0;

    while (completed < MFS_BLOCK_SIZE) {
        off_t offset = start + (off_t)completed;

        ssize_t transferred = pwrite(
            adapter->fd,
            buffer + completed,
            MFS_BLOCK_SIZE - completed,
            offset
        );

        if (transferred < 0) {
            if (errno == EINTR)
                continue;

            return MFS_IO_ERROR;
        }

        if (transferred == 0)
            return MFS_IO_ERROR;

        completed += (size_t)transferred;
    }

    return MFS_IO_OK;
}

static enum mfs_io_result
posix_flush(void *context)
{
    struct mfs_posix_block_device *adapter = context;

    if (fsync(adapter->fd) != 0)
        return MFS_IO_ERROR;

    return MFS_IO_OK;
}

enum mfs_io_result
mfs_posix_block_device_bind(struct mfs_posix_block_device *adapter,
                            struct mfs_block_device *device)
{
    if (adapter == NULL || device == NULL)
        return MFS_IO_INVALID_ARGUMENT;

    if (adapter->fd < 0 || adapter->block_count < 2)
        return MFS_IO_INVALID_ARGUMENT;

    struct mfs_block_device bound = {
        .context = adapter,
        .block_count = adapter->block_count,
        .read = posix_read,
        .write = posix_write,
        .flush = posix_flush
    };

    *device = bound;

    return MFS_IO_OK;
}
