# MFS Architecture

> **Status:** initial design; interfaces and disk structures are not yet frozen.

## Purpose

MFS (MyOS File System) is the native persistent filesystem intended for MyOS installations. MFS is developed and tested independently of the MyOS kernel. This document defines ownership, component boundaries, and design invariants; it is not an assertion that the described components are implemented.

The [on-disk format specification](on-disk-format.md) owns the byte-level persistent contract. The [development plan](development-plan.md) defines incremental implementation checkpoints.

## Goals

- A documented, versioned, portable filesystem format.
- One platform-neutral implementation usable by host tools and MyOS.
- Explicit, testable block I/O and persistence boundaries.
- A small filesystem core with understandable inode, directory, and allocation rules.
- Transactional metadata updates, metadata journaling, deterministic recovery, and crash-injection validation before writable MFS is considered stable.
- Strict validation of malformed media and predictable failure behavior.
- Native MyOS integration through generic block and VFS interfaces.

## Non-goals

The first stable MFS does not require snapshots, compression, encryption, deduplication, RAID, distributed storage, B-tree directory indexes, or production-scale optimization. MFS is not an ext4 or Linux ABI clone. An MFS-specific syscall or application workaround is not an acceptable substitute for a generic MyOS VFS contract.

## Ownership and dependencies

```text
Host applications                MyOS applications
mkfs.mfs / mfsdump / fsck.mfs    POSIX-like file APIs
          |                             |
          v                             v
      MFS library                  MyOS VFS / FD
          |                             |
          |                       MFS VFS adapter
          |                             |
          +--------- MFS core ----------+
                       |
              block I/O contract
                 /          \
         POSIX backend    MyOS block backend
                 |              |
        image / raw device  generic block device
```

The MFS repository owns the persistent format, filesystem-specific algorithms, library interfaces, journal, recovery, host utilities, and reusable tests. The MyOS repository owns VFS and pathname semantics, descriptor lifetime, mount lifecycle, device discovery, storage drivers, block-device contracts, kernel synchronization, and the kernel-facing filesystem adapter.

An integration adapter must not force MyOS's VFS to expose MFS-specific inode layouts. The core must not include private MyOS kernel headers, use POSIX system calls directly, or assume the presence of a particular disk controller.

## Repository modules

| Path | Responsibility |
| --- | --- |
| `include/mfs/` | Public MFS library headers and explicitly exported contracts |
| `lib/format/` | On-disk encoding, decoding, validation, version/feature interpretation |
| `lib/allocation/` | Free-space accounting and block allocation |
| `lib/inode/` | Filesystem-owned inode metadata and file block mapping |
| `lib/directory/` | Directory entry representation and operations |
| `lib/journal/` | Transaction records, ordering, commit protocol |
| `lib/recovery/` | Journal replay, consistency checks, failure handling |
| `platform/posix/` | Host file/raw-device block I/O implementation |
| `platform/myos/` | MyOS-side MFS platform integration, distinct from generic VFS ownership |
| `tools/` | Format, inspect, verify, and later repair filesystem images |
| `tests/` | Unit, integration, corruption, and interrupted-write tests |

The directories exist today as scaffolding. No functionality should be inferred from their names.

## I/O contract

The filesystem core addresses an MFS volume through volume-relative logical block numbers. It does not own partition-table parsing or host device naming. A caller selects a whole test image, a dedicated partition, or another bounded region and exposes that region as an MFS volume.

The minimal platform contract must eventually provide:

- capacity and logical sector/alignment constraints;
- bounded, checked reads and writes with explicit offsets or block numbers;
- a durability barrier/flush whose success meaning is documented;
- deterministic error reporting, including short I/O handling;
- read-only versus writable access and explicit ownership of the underlying handle.

The actual C signatures, synchronization model, and allocation policy are **TBD**. For crash safety, write completion and durable persistence must not be treated as equivalent. Backends that cannot provide the required ordering guarantees must not enable unsafe writable operation.

Host binaries on macOS are native Mach-O executables; MyOS binaries are ELF. The shared artifact is the portable C library source and its on-disk contract, not a cross-platform executable.

## Persistent representation

All on-disk fields have documented offsets, widths, byte order, and validity conditions. Encoders and decoders operate on byte buffers; dumping in-memory C structs directly to media is prohibited. Any arithmetic involving offsets, counts, or sizes is overflow-checked before I/O.

An unknown incompatible format or unsupported required feature causes a safe failure. Development images may be discarded until a stable format is declared. The initial block size, superblock placement, inode encoding, journal layout, checksums, and compatibility bit definitions remain **TBD** in the on-disk specification.

## Metadata transactions and recovery

Journaling is an architectural requirement, even though the first formatting and read-only milestones precede its implementation.

The initial intended journal scope is **metadata**. The journal and recovery design must establish ordering so that committed metadata cannot expose stale or uninitialized file data. Journal replay must be deterministic and, where applicable, idempotent. An incomplete transaction must never be mistaken for a committed one.

The precise transaction records, checksum coverage, barriers, checkpoint policy, and replay algorithm are design questions, not current implementation promises. Until journaling and failure-injection tests validate these invariants, writes are experimental and filesystem images are disposable.

## Safety and failure handling

Readers validate magic/version, sizes, bounds, pointers, allocation metadata, directory lengths, checksums where defined, and relationships between persistent structures before trusting them. Corrupt images must not cause out-of-bounds reads or writes.

All first-stage destructive tests target regular files or disposable loopback media. Raw-device formatting requires an explicit opt-in, clear target identification, and safeguards against overwriting mounted or system volumes. Do not interpret successful host formatting as proof that macOS can mount MFS: native macOS mount support is a separate undertaking.

## Design discipline

Design documents use English. Cross-module headers use canonical `<mfs/...>` paths, and local private headers remain local. Changes should be small, have tests and documented invariants, and preserve the separation between platform code and the filesystem core. Decisions affecting disk compatibility must be recorded before implementation is relied upon.
