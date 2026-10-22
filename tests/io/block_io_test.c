// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (C) 2026 Cristian Recabarren
 */

#include <mfs/block_io.h>
#include <mfs/assert.h>

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#define TEST_BLOCK_COUNT 4U

struct memory_device {
    uint8_t blocks[TEST_BLOCK_COUNT][MFS_BLOCK_SIZE];

    unsigned read_calls;
    unsigned write_calls;
    unsigned flush_calls;

    enum mfs_io_result read_result;
    enum mfs_io_result write_result;
    enum mfs_io_result flush_result;
};

static enum mfs_io_result
memory_read(void *context, uint64_t block, uint8_t *buffer)
{
    struct memory_device *memory = context;

    memory->read_calls++;

    if (memory->read_result != MFS_IO_OK)
        return memory->read_result;

    for (size_t i = 0; i < MFS_BLOCK_SIZE; ++i)
        buffer[i] = memory->blocks[block][i];

    return MFS_IO_OK;
}

static enum mfs_io_result
memory_write(void *context, uint64_t block, const uint8_t *buffer)
{
    struct memory_device *memory = context;

    memory->write_calls++;

    if (memory->write_result != MFS_IO_OK)
        return memory->write_result;

    for (size_t i = 0; i < MFS_BLOCK_SIZE; ++i)
        memory->blocks[block][i] = buffer[i];

    return MFS_IO_OK;
}

static enum mfs_io_result
memory_flush(void *context)
{
    struct memory_device *memory = context;

    memory->flush_calls++;

    return memory->flush_result;
}

static struct mfs_block_device
make_block_device(struct memory_device *memory)
{
    struct mfs_block_device device = {
        .context = memory,
        .block_count = TEST_BLOCK_COUNT,
        .read = memory_read,
        .write = memory_write,
        .flush = memory_flush
    };

    return device;
}


static void test_read_write_roundtrip(void)
{
    struct memory_device memory = {0};
    struct mfs_block_device device = make_block_device(&memory);

    uint8_t written[MFS_BLOCK_SIZE];
    uint8_t readback[MFS_BLOCK_SIZE] = {0};

    for (size_t i = 0; i < MFS_BLOCK_SIZE; ++i)
        written[i] = (uint8_t)(i & 0xFFU);

    MFS_TEST_ASSERT(mfs_block_write(
        &device, 2, written) == MFS_IO_OK);

    MFS_TEST_ASSERT(mfs_block_read(
        &device, 2, readback) == MFS_IO_OK);

    for (size_t i = 0; i < MFS_BLOCK_SIZE; ++i)
        MFS_TEST_ASSERT(readback[i] == written[i]);

    MFS_TEST_ASSERT(memory.write_calls == 1);
    MFS_TEST_ASSERT(memory.read_calls == 1);

    printf("PASS: block read/write roundtrip\n");
}

static void test_block_boundaries(void)
{
    struct memory_device memory = {0};
    struct mfs_block_device device = make_block_device(&memory);

    uint8_t buffer[MFS_BLOCK_SIZE] = {0};

    /*
     * Blocks 0 through 3 are valid.
     * Block 4 is outside the device.
     */

    MFS_TEST_ASSERT(mfs_block_read(
        &device, TEST_BLOCK_COUNT, buffer)
        == MFS_IO_OUT_OF_RANGE);

    MFS_TEST_ASSERT(mfs_block_write(
        &device, UINT64_MAX, buffer)
        == MFS_IO_OUT_OF_RANGE);

    MFS_TEST_ASSERT(memory.read_calls == 0);
    MFS_TEST_ASSERT(memory.write_calls == 0);

    printf("PASS: block boundaries\n");
}

static void test_flush(void)
{
    struct memory_device memory = {0};
    struct mfs_block_device device = make_block_device(&memory);

    MFS_TEST_ASSERT(mfs_block_flush(&device) == MFS_IO_OK);
    MFS_TEST_ASSERT(memory.flush_calls == 1);

    printf("PASS: block flush\n");
}

static void test_null_arguments(void)
{
    struct memory_device memory = {0};
    struct mfs_block_device device = make_block_device(&memory);
    uint8_t buffer[MFS_BLOCK_SIZE] = {0};

    MFS_TEST_ASSERT(mfs_block_read(
        NULL, 0, buffer) == MFS_IO_INVALID_ARGUMENT);

    MFS_TEST_ASSERT(mfs_block_read(
        &device, 0, NULL) == MFS_IO_INVALID_ARGUMENT);

    MFS_TEST_ASSERT(mfs_block_write(
        NULL, 0, buffer) == MFS_IO_INVALID_ARGUMENT);

    MFS_TEST_ASSERT(mfs_block_write(
        &device, 0, NULL) == MFS_IO_INVALID_ARGUMENT);

    MFS_TEST_ASSERT(mfs_block_flush(
        NULL) == MFS_IO_INVALID_ARGUMENT);

    MFS_TEST_ASSERT(memory.read_calls == 0);
    MFS_TEST_ASSERT(memory.write_calls == 0);
    MFS_TEST_ASSERT(memory.flush_calls == 0);

    printf("PASS: null arguments rejected\n");
}

static void test_missing_callbacks(void)
{
    struct memory_device memory = {0};
    struct mfs_block_device device = make_block_device(&memory);
    uint8_t buffer[MFS_BLOCK_SIZE] = {0};

    device.read = NULL;

    MFS_TEST_ASSERT(mfs_block_read(
        &device, 0, buffer) == MFS_IO_INVALID_ARGUMENT);

    device.write = NULL;

    MFS_TEST_ASSERT(mfs_block_write(
        &device, 0, buffer) == MFS_IO_INVALID_ARGUMENT);

    device.flush = NULL;

    MFS_TEST_ASSERT(mfs_block_flush(
        &device) == MFS_IO_INVALID_ARGUMENT);

    MFS_TEST_ASSERT(memory.read_calls == 0);
    MFS_TEST_ASSERT(memory.write_calls == 0);
    MFS_TEST_ASSERT(memory.flush_calls == 0);

    printf("PASS: missing callbacks rejected\n");
}

static void test_backend_errors(void)
{
    struct memory_device memory = {0};
    struct mfs_block_device device = make_block_device(&memory);
    uint8_t buffer[MFS_BLOCK_SIZE] = {0};

    memory.read_result = MFS_IO_ERROR;
    memory.write_result = MFS_IO_ERROR;
    memory.flush_result = MFS_IO_ERROR;

    MFS_TEST_ASSERT(mfs_block_read(
        &device, 0, buffer) == MFS_IO_ERROR);

    MFS_TEST_ASSERT(mfs_block_write(
        &device, 0, buffer) == MFS_IO_ERROR);

    MFS_TEST_ASSERT(mfs_block_flush(
        &device) == MFS_IO_ERROR);

    MFS_TEST_ASSERT(memory.read_calls == 1);
    MFS_TEST_ASSERT(memory.write_calls == 1);
    MFS_TEST_ASSERT(memory.flush_calls == 1);

    printf("PASS: backend errors propagated\n");
}

int main(void)
{
    test_read_write_roundtrip();
    test_block_boundaries();
    test_flush();

    test_null_arguments();
    test_missing_callbacks();
    test_backend_errors();

    printf("All block I/O tests passed.\n");

    return 0;
}
