# MFS On-Disk Format — Allocation Bitmap v0.2 (Draft)

> **Status:** design proposal, not implemented. This document does not modify the experimental MFS v0.1 recognition-only format.

## Purpose and scope

This document proposes the first persistent block allocation metadata for MFS.

The proposed v0.2 revision introduces an allocation bitmap identifying physical blocks as allocated or free. It does not define inodes, directories, file contents, allocation transactions, journal recovery, or mount semantics.

## Compatibility

MFS v0.1 remains unchanged.

MFS v0.2 is a distinct experimental on-disk revision. Existing v0.1 decoders must reject v0.2 images as unsupported.

No automatic format migration is specified.

## Volume geometry

- Logical block size: 4096 bytes.
- Block numbering: zero-based.
- Block 0: reserved.
- Block 1: primary superblock.
- Block 2 onward: allocation bitmap, followed by initially free blocks.
- Minimum volume size: 4 blocks.
- Volume size must equal `total_blocks * 4096` bytes.

## Allocation bitmap

Each bit represents exactly one physical block.

- Bit value `0`: free.
- Bit value `1`: allocated or reserved.
- Bit numbering within a byte is least-significant-bit first.
- The bitmap covers every physical block, including reserved blocks and the bitmap itself.
- Bitmap blocks form a contiguous extent beginning at physical block 2.

A free bit indicates that a physical block is available for future allocation. It does not imply that its underlying bytes are zero-filled or contain valid filesystem data.

For a physical block number `n`:

- Byte index: `n / 8`.
- Bit index: `n % 8`.
- Bit mask: `1 << (n % 8)`.

Each 4096-byte bitmap block represents 32768 physical blocks.

The required bitmap length is:

`ceil(total_blocks / 32768)` physical blocks.

## Initial allocation state

The formatter must mark as allocated:

- Physical block 0.
- Physical block 1.
- Every block occupied by the allocation bitmap.

Every remaining physical block is initially free.

All bitmap bits beyond the actual volume capacity must be set to `1`, including unused bytes in the last bitmap block.

## Geometry and reference vectors

Each allocation bitmap block contains 32768 bits.

For `total_blocks = N`, the bitmap length in physical blocks is:

`allocation_blocks = N / 32768 + (N % 32768 != 0)`

The calculation must use checked unsigned arithmetic and must not rely on adding 32767 to `N`.

The allocation bitmap starts at block 2. Its last block is `2 + allocation_blocks - 1`.

The first initially free block is `2 + allocation_blocks`.

The formatter must reject geometries where fewer than one initially free physical block would remain.

| Total blocks | Allocation blocks | First free block | Initially free blocks |
| ---: | ---: | ---: | ---: |
| 4 | 1 | 3 | 1 |
| 32768 | 1 | 3 | 32765 |
| 32769 | 2 | 4 | 32765 |
| 65536 | 2 | 4 | 65532 |
| 65537 | 3 | 5 | 65532 |

### Initial bitmap encoding

The initial allocation bitmap must have every bit set to `1`, except the bits corresponding to initially free physical blocks.

All byte offsets below are relative to the beginning of the allocation bitmap, not the beginning of the volume.

| Total blocks | Bitmap byte offset | Expected value | Description |
| ---: | ---: | --- | --- |
| 4 | 0 | `0xF7` | Blocks 0–2 allocated; block 3 free; remaining bits outside volume |
| 4 | 1–4095 | `0xFF` | All remaining bits outside volume |
| 32768 | 0 | `0x07` | Blocks 0–2 allocated |
| 32768 | 1–4095 | `0x00` | Remaining physical blocks free |
| 32769 | 0 | `0x0F` | Blocks 0–3 allocated, including both bitmap blocks |
| 32769 | 1–4095 | `0x00` | Physical blocks 8–32767 free |
| 32769 | 4096 | `0xFE` | Block 32768 free; blocks 32769–32775 outside volume |
| 32769 | 4097–8191 | `0xFF` | All remaining bits outside volume |
| 65536 | 0 | `0x0F` | Blocks 0–3 allocated |
| 65536 | 1–8191 | `0x00` | Remaining physical blocks free |

These are deterministic bitmap encoding vectors, not complete volume-image fixtures.

A conforming formatter must produce exactly these bytes for the specified initial allocation states.

## Formatting and persistence ordering

The initial v0.2 formatter must:

1. Create an explicitly named disposable regular-file image exclusively.
2. Establish its exact logical volume size.
3. Ensure reserved block 0 contains only zero bytes.
4. Construct and write every allocation bitmap block.
5. Flush the allocation metadata.
6. Encode and write the v0.2 superblock with its CRC32C.
7. Flush the superblock.
8. Close the image.

The superblock must not be published before successful completion of the allocation bitmap write and flush.

Failures must be reported. An unsuccessful formatting operation must not be advertised as a valid completed volume.

This ordering is not a transaction protocol and does not guarantee power-failure atomicity, crash recovery, or successful rollback.

## Additional verification requirements

A future v0.2 checker must validate the entire allocation bitmap extent.

The checker must reject:

- Reserved or bitmap blocks marked free.
- Any bit beyond `total_blocks` that is not marked allocated.
- Incorrect allocation bitmap length or location.
- Allocation extents extending beyond volume capacity.
- Unsupported revision or feature combinations.

A checker must not infer that arbitrary allocated data blocks contain valid files or metadata, because these structures are not yet defined.

## Superblock fields

The v0.2 proposal retains the v0.1 superblock field offsets.

- `format_major`: 0.
- `format_minor`: 2.
- `allocation_start`: 2.
- `allocation_blocks`: computed bitmap length.
- `total_blocks`: actual volume block count.
- `superblock_crc32c`: CRC32C over the complete 4096-byte superblock, with its checksum field logically zeroed.

All currently unused journal, inode, and feature fields remain zero.

### Format state

For revision 0.2, `format_state = 1` identifies an experimental MFS volume.

The field does not encode filesystem capabilities, allocation activity, transaction state, mountability, or recovery status.

The on-disk revision determines which metadata structures and invariants are required.

Revision 0.1 retains its original recognition-only semantics. The revision 0.2 interpretation does not retroactively alter the revision 0.1 contract.

No additional `format_state` values are defined for revision 0.2. Unknown values must be rejected.

## Consistency invariants

A future v0.2 consistency checker must verify:

1. Valid superblock version and checksum.
2. Exact agreement between volume capacity and `total_blocks`.
3. Valid allocation bitmap extent without arithmetic overflow.
4. Allocation bitmap length sufficient to represent all physical blocks.
5. All reserved and metadata blocks marked allocated.
6. All out-of-volume bitmap bits marked allocated.
7. No unsupported feature flags or unimplemented metadata structures.

## Deferred decisions

- Allocation and deallocation API.
- Transactional crash consistency and recovery guarantees.
- Journal integration.
- Bitmap redundancy and checksums.
- Metadata allocation for inode structures.
- Root directory and initial filesystem layout.
- Future compatibility and migration rules.

No write-capable filesystem operation is authorized by this draft.