# MFS Development Plan

> **Status:** proposed implementation sequence; no milestones below are claimed complete.

## Strategy

Build MFS as a host-testable filesystem first, then integrate it with MyOS's generic block and VFS layers. The repository layout is retained. Develop vertical slices that produce observable, independently verifiable results. Use regular disk-image files before raw physical devices.

The authoritative MyOS integration roadmap is [MyOS epic #131](https://github.com/crecabar/myos/issues/131), including the child issues for format/tooling, block/VFS integration, read-only core, writable core, cache, journal, and crash recovery.

## Stage 0 — Document contracts

- Agree on architecture ownership and block I/O semantics.
- Specify the first minimal **experimental** on-disk image with exact byte layouts, byte order, block size, offsets, and validation rules.
- Record open decisions explicitly rather than writing guessed structures into C headers.
- Define image fixtures, build conventions, and test execution targets.

**Exit criterion:** an implementer can encode and decode an initial superblock without guessing any on-disk byte or required compatibility check.

## Stage 1 — Host image recognition

Implement a regular-file backend in `platform/posix/`, basic encode/decode in `lib/format/`, and narrow first versions of `mkfs.mfs` and `mfsdump`.

A test creates a disposable image, formats it, closes it, reopens it through a separate path, validates the superblock, and rejects deliberately corrupted or truncated input. `fsck.mfs` may initially verify only the structures actually specified and must report that limitation rather than claiming full filesystem consistency.

**Exit criterion:** reproducible format/reopen/inspect and corruption-rejection tests pass on development hosts.

## Stage 2 — Read-only namespace

Implement allocation metadata, a root inode, directory entry decoding, and regular-file reads as the format is extended. Tests must exercise reopened images, unknown entries, invalid block references, and truncated metadata.

**Exit criterion:** tools can list and read known fixtures from a fresh, independently reopened image.

## Stage 3 — Experimental write path

Implement allocation, file and directory creation, update, and removal with explicit resource-ownership rules. Test persistence across reopen and crash-like injected I/O failures. This is still an **experimental, disposable-image-only** write path until transaction and recovery invariants have been proven.

**Exit criterion:** basic writes survive normal close/reopen and every injected error leaves a diagnosable state. This is not a claim of crash consistency.

## Stage 4 — Transactions, journaling, recovery

Implement metadata transaction boundaries, write ordering and durability barriers, journal commit and checkpoint rules, replay, and failure injection at each relevant write/flush boundary. Add offline consistency checks and narrowly scoped repair operations with clear safety rules.

**Exit criterion:** committed transactions survive interruption and incomplete transactions cannot publish corrupt references, demonstrated by repeatable crash/reopen/recovery tests. Read/write release readiness requires these tests.

## Stage 5 — MyOS integration

Bind the reusable core to MyOS block I/O and the generic VFS adapter, initially read-only. Add write support only when cache, flush, synchronization, and journal contracts are satisfied. Use QEMU disk images before attempting dedicated removable media.

**Exit criterion:** an image prepared by host tools is readable through ordinary MyOS file descriptors; eventually MyOS writes can be validated using independent host-side tools.

## Raw-device policy

`mkfs.mfs` must not accept a physical disk as a routine stage-1 test. Before enabling raw-device formatting, implement device identity/capacity display, mounted/system-volume detection where supported, explicit destructive confirmation, target-range enforcement, and a documented recovery/backup procedure. Images are the default throughout development.

## Working practices

Use English in specifications, code documentation, test names, commit messages, issues, and pull requests. Keep assertions about current implementation distinct from design proposals. Do not add kernel-specific behavior to the portable library merely to satisfy one integration test. Do not declare a stable on-disk revision until versioning, compatibility, recovery, and verification are proven.
