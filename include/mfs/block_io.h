// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (C) 2026 Cristian Recabarren
 */

#ifndef MFS_BLOCK_IO_H
#define MFS_BLOCK_IO_H

#include <stdint.h>

#include <mfs/format.h>

/*
 * Logical block I/O results.
 */
enum mfs_io_result {
    MFS_IO_OK = 0,
    MFS_IO_INVALID_ARGUMENT,
    MFS_IO_OUT_OF_RANGE,
    MFS_IO_ERROR
};

/*
 * Portable block device abstraction.
 *
 * Block indices are relative to the beginning of the
 * selected MFS volume.
 *
 * Every successful read or write transfers exactly
 * MFS_BLOCK_SIZE bytes.
 *
 * Failed writes may have partially modified storage.
 */
struct mfs_block_device {
    void *context;
    uint64_t block_count;

    enum mfs_io_result (*read)(
        void *context,
        uint64_t block,
        uint8_t *buffer
    );

    enum mfs_io_result (*write)(
        void *context,
        uint64_t block,
        const uint8_t *buffer
    );

    /*
     * Request persistence of completed writes.
     *
     * Durability depends on backend/device guarantees.
     */
    enum mfs_io_result (*flush)(void *context);
};

/*
 * Checked operations implemented by the portable core.
 */
enum mfs_io_result mfs_block_read(
    const struct mfs_block_device *device,
    uint64_t block,
    uint8_t *buffer
);

enum mfs_io_result mfs_block_write(
    const struct mfs_block_device *device,
    uint64_t block,
    const uint8_t *buffer
);

enum mfs_io_result mfs_block_flush(
    const struct mfs_block_device *device
);

#endif /* MFS_BLOCK_IO_H */
