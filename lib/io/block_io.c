// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (C) 2026 Cristian Recabarren
 */

#include <mfs/block_io.h>
#include <stddef.h>

enum mfs_io_result
mfs_block_read(const struct mfs_block_device *device,
               uint64_t block,
               uint8_t *buffer)
{
    if (device == NULL || buffer == NULL)
        return MFS_IO_INVALID_ARGUMENT;

    if (device->read == NULL)
        return MFS_IO_INVALID_ARGUMENT;

    if (block >= device->block_count)
        return MFS_IO_OUT_OF_RANGE;

    return device->read(device->context, block, buffer);
}

enum mfs_io_result
mfs_block_write(const struct mfs_block_device *device,
                uint64_t block,
                const uint8_t *buffer)
{
    if (device == NULL || buffer == NULL)
        return MFS_IO_INVALID_ARGUMENT;

    if (device->write == NULL)
        return MFS_IO_INVALID_ARGUMENT;

    if (block >= device->block_count)
        return MFS_IO_OUT_OF_RANGE;

    return device->write(device->context, block, buffer);
}

enum mfs_io_result
mfs_block_flush(const struct mfs_block_device *device)
{
    if (device == NULL)
        return MFS_IO_INVALID_ARGUMENT;

    if (device->flush == NULL)
        return MFS_IO_INVALID_ARGUMENT;

    return device->flush(device->context);
}
