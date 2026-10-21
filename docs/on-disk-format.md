# MFS On-Disk Format — Experimental Superblock v0

> **Status:** proposed byte-exact contract for checkpoint 4; review before implementing. The experimental format is not stable and is not a mountable filesystem.

## Purpose and scope

This document defines an **experimental recognition-only** MFS image and its first superblock. It is sufficient to implement deterministic encoding, independent decoding, inspection, and rejection of malformed image headers. It does **not** define inodes, directories, allocation, data access, a mountable root, transactional updates, or recovery. A valid v0 header must never be interpreted as evidence that a complete filesystem exists.

The implementation version (`<mfs/version.h>`, currently 0.1.0) and the on-disk revision (here 0.1) are independent.

## Volume geometry

- A volume is the byte range explicitly selected by the caller; offsets are **relative to that range**, not to an entire physical disk or partition table.
- Each logical block is exactly **4096 bytes** for this experimental revision.
- Block numbers are zero-based; the byte offset of block `n` is `n * 4096`, with checked multiplication.
- The volume has at least **2 complete blocks**. Its length is an exact multiple of 4096 and equals `total_blocks * 4096`; a regular-file image with extra trailing bytes is rejected for this revision.
- Block **0** (bytes 0–4095) is reserved and must be all zero in an image produced by the first formatter. A recognizer does not use block 0 as a signature or require it to be zero, to permit future bootstrapping work.
- Block **1** (bytes **4096–8191**) contains the sole v0 superblock, occupying its entire 4096-byte block. No backup superblock exists in v0.
- Blocks 2 onward are **unallocated/undefined** in this recognition-only revision. The formatter must initialize the superblock and block 0; it must not claim to initialize filesystem metadata that is not specified.

A formatter must create or accept only an explicitly named disposable regular-file image in the first implementation slice. Raw-device formatting, whole-disk partitioning, and OS-native mounting are out of scope.

## Encoding conventions

- Every multi-byte integer is **unsigned little-endian**. Byte arrays retain the listed byte order.
- Integers and fields are encoded/decoded explicitly from a byte buffer; `memcpy` of a native C struct to disk is not a valid serialization strategy.
- Every unassigned byte in the 4096-byte superblock **must be zero** when writing and when validating this v0 revision.
- All sizes, ranges, and derived offsets must be checked for integer overflow before use.
- The CRC field is written as little-endian, like any other `u32`.

## Superblock v0.1 field map

Offsets are **relative to the start of block 1** (volume byte offset 4096). Ranges are half-open.

| Offset | Size | Type | Field | Required encoding / meaning |
| ---: | ---: | --- | --- | --- |
| 0 | 8 | bytes | `magic` | `4D 59 4F 53 4D 46 53 00` (`MYOSMFS\0`) |
| 8 | 2 | u16 | `format_major` | `0` |
| 10 | 2 | u16 | `format_minor` | `1` |
| 12 | 2 | u16 | `header_bytes` | `128` |
| 14 | 2 | u16 | `reserved_0` | `0` |
| 16 | 4 | u32 | `block_bytes` | `4096` |
| 20 | 4 | u32 | `format_state` | `1` = experimental, recognition only |
| 24 | 8 | u64 | `total_blocks` | `>= 2`, exact volume size / 4096 |
| 32 | 16 | bytes | `volume_uuid` | 16 opaque bytes, not all zero |
| 48 | 8 | u64 | `compat_features` | `0`; no defined flags |
| 56 | 8 | u64 | `ro_compat_features` | `0`; no defined flags |
| 64 | 8 | u64 | `incompat_features` | `0`; no defined flags |
| 72 | 8 | u64 | `journal_start` | `0`; journal not implemented |
| 80 | 8 | u64 | `journal_blocks` | `0`; journal not implemented |
| 88 | 8 | u64 | `root_inode` | `0`; inode model not implemented |
| 96 | 8 | u64 | `allocation_start` | `0`; allocation map not implemented |
| 104 | 8 | u64 | `allocation_blocks` | `0`; allocation map not implemented |
| 112 | 4 | u32 | `superblock_crc32c` | CRC32C of entire 4096-byte block, with these four bytes treated as zero |
| 116 | 12 | bytes | `reserved_1` | all zero |
| 128 | 3968 | bytes | `reserved_tail` | all zero |

Fields reserved for future journal/root/allocation structures are **not active descriptors**: any nonzero value is invalid under revision 0.1. Future formats must explicitly revise this contract. The presence of feature-bit fields does not establish support for any feature in v0.1; all three masks must be zero.

## CRC32C

Use CRC-32C (Castagnoli), **not** IEEE CRC-32. Parameters: polynomial `0x1EDC6F41` (reflected `0x82F63B78`), init `0xFFFFFFFF`, input/output reflected, final xor `0xFFFFFFFF`, no other salt. Compute over all **4096 bytes** of block 1, after logically replacing bytes [112,116) with zero. Store the resulting 32-bit value at offset 112 in little-endian order.

The checksum detects accidental modification; it is not a cryptographic authenticity check. A valid checksum does not make other superblock fields trustworthy.

## Reference encoding vector

A reproducible **16,384-block (64 MiB)** fixture has `total_blocks=16384`, UUID bytes `00 01 ... 0F`, and all other fields as specified above. For this vector:

- Bytes [0,48) of block 1 are:

```text
4D 59 4F 53 4D 46 53 00  00 00 01 00 80 00 00 00
00 10 00 00 01 00 00 00  00 40 00 00 00 00 00 00
00 01 02 03 04 05 06 07  08 09 0A 0B 0C 0D 0E 0F
```

- Bytes [48,112) and [116,4096) are all zero.
- CRC32C calculated with [112,116) zeroed: **`0x0737580F`**.
- Encoded bytes [112,116): **`0F 58 37 07`**.

This vector is a specification fixture, not a claim that any image file has been generated or validated by the project's build.

## Recognition and validation

A v0.1 image recognizer must:

1. Obtain the exact caller-selected volume capacity and verify at least 8192 bytes, block alignment, and checked `total_blocks` arithmetic; do not read beyond capacity.
2. Read the full block at volume offset 4096; treat short I/O as failure.
3. Verify the exact magic, **both** format version fields, `header_bytes`, block size, and experimental state.
4. Reject all nonzero reserved bytes, nonzero feature masks, nonzero unimplemented structure pointers/counts, and an all-zero UUID.
5. Verify `total_blocks >= 2`, no overflow in `total_blocks * 4096`, and exact agreement with the exposed volume capacity.
6. Verify CRC32C as defined above.
7. Report **“experimental MFS v0.1 superblock recognized”**, never “filesystem clean”, “mountable”, or “journal recovered”.

A decoder should distinguish malformed/corrupt data, unsupported format revision, and backend I/O error. No structure pointer may be followed on this revision. The validation order must not permit unchecked arithmetic or out-of-bounds reads; CRC may be checked before or after semantic checks provided the decoder remains safe.

## Compatibility policy for the experimental revision

- Exact major **0**, minor **1** support is required; all other versions are unsupported, including a newer minor version.
- Unknown feature bits, including so-called compatible bits, are rejected because no feature semantics have yet been defined.
- No read/write mount and no crash recovery may be advertised for v0.1.
- This revision is **disposable and incompatible by default** with future development formats; compatibility must be deliberately specified later.
- No transaction record or journal commitment is implied by the placeholder region fields.

## Test matrix for checkpoint 5

| Test | Expected result |
| --- | --- |
| Encode deterministic vector and compare all 4096 bytes / CRC | exact match |
| Decode fixture through an independently initialized buffer | recognized experimental superblock |
| Reopen valid regular-file image | recognized |
| Truncated image / short superblock read | reject |
| Size not divisible by 4096 | reject |
| Wrong magic, header size, block size, version or state | reject |
| All-zero UUID / any nonzero reserved byte | reject |
| Any nonzero feature mask or inactive region field | reject |
| Image capacity disagrees with `total_blocks` | reject |
| One-bit change without recomputing CRC | reject |
| CRC changed but all other bytes valid | reject |
| `total_blocks` overflow relative to supported I/O offset type | reject safely |

A host-only parser should operate on an explicit buffer and a caller-supplied volume extent; actual file I/O belongs in the POSIX adapter. Neither a successful header test nor a successful `mfsdump` inspection is equivalent to a full `fsck.mfs` pass.

## Deferred decisions

Inodes, directories, free-space maps, extent mapping, additional superblocks, journaling record layout, durability barriers, feature negotiation for stable revisions, timestamps, naming rules, and actual mount semantics require separate design slices. A later revision must decide which fields remain compatible and how an upgrade is represented.

## Related documents

- [Architecture](architecture.md)
- [Initial design decisions](format-design-decisions.md)
- [Build and C runtime boundaries](build-system.md)
- [Header include policy](include-policy.md)
