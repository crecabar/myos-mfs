// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (C) 2026 Cristian Recabarren
 */

#ifndef MFS_POSIX_BLOCK_DEVICE_H
#define MFS_POSIX_BLOCK_DEVICE_H

#include <stdint.h>

#include <mfs/block_io.h>

/*
 * POSIX block device adapter.
 *
 * The caller owns the file descriptor and must keep it
 * open for the lifetime of the adapter.
 *
 * The adapter does not close the descriptor.
 */
struct mfs_posix_block_device {
    int fd;
    uint64_t block_count;
};

/*
 * Initialize an adapter for an already opened regular file.
 *
 * The file size must be a positive multiple of
 * MFS_BLOCK_SIZE.
 *
 * Initialization does not transfer descriptor ownership.
 */
enum mfs_io_result mfs_posix_block_device_init(
    struct mfs_posix_block_device *adapter,
    int fd
);

/*
 * Initialize the portable device interface.
 *
 * The adapter must remain alive while the returned
 * device is in use.
 */
enum mfs_io_result mfs_posix_block_device_bind(
    struct mfs_posix_block_device *adapter,
    struct mfs_block_device *device
);

#endif /* MFS_POSIX_BLOCK_DEVICE_H */
