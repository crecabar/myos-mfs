# myos-mfs

Native journaled filesystem for MyOS.

## Purpose

`myos-mfs` is the reference project for **MFS (MyOS File System)**, the native persistent filesystem designed for MyOS.

MFS is a requirement of the MyOS North Star for native installation and eventual self-hosting, but the filesystem itself is developed as a project separate from the MyOS kernel.

## Architectural boundary

```text
MyOS userspace
      |
      v
MyOS VFS / file descriptors
      |
      v
kernel MFS adapter
      |
      v
  MFS core / format
      |
      v
block-device interface
```

The `crecabar/myos` repository owns the generic kernel contracts needed to mount and operate filesystems:

- VFS and pathname semantics;
- file/open-file lifetime;
- mount infrastructure;
- block-device interfaces;
- device discovery and storage drivers;
- kernel-side synchronization and integration adapters.

This repository owns the parts that are specifically MFS:

- on-disk format and versioning;
- superblock, inode and directory representation;
- block/free-space allocation;
- transaction model;
- metadata journal;
- persistence and ordering rules;
- recovery and consistency invariants;
- checksums and feature flags;
- host-side filesystem creation, inspection and repair tools;
- corruption, crash and recovery test tooling.

The boundary is intentional: VFS must not be designed around MFS internals, and MFS must not depend on private implementation details of one storage controller.

## Design principles

MFS development follows a few core rules:

- the on-disk format is documented and versioned;
- writable metadata operations are transactional;
- journaling is an architectural requirement, not a later add-on;
- crash consistency must be demonstrated through failure-injection tests;
- recovery must be deterministic and, where applicable, idempotent;
- committed metadata must never expose stale or uninitialized file data;
- malformed or corrupt filesystem structures must fail safely;
- filesystem tooling should be usable on development hosts without booting MyOS;
- kernel integration uses general VFS/block contracts rather than MFS-specific shortcuts.

## Initial journaling target

The first journal implementation is intended to protect **filesystem metadata**.

File data does not initially require full data journaling, but ordering guarantees must prevent committed metadata from referencing stale or uninitialized blocks.

## Tooling direction

A substantial part of MFS should be testable independently from the kernel.

Expected tooling eventually includes equivalents of:

```text
mkfs.mfs
fsck.mfs
mfsdump
filesystem-image test tools
failure/corruption injection tools
```

This allows the format, journal and recovery logic to be exercised against disk images before the same filesystem is mounted by MyOS.

## Relationship to MyOS

MFS does **not** block the first `MyOS $` shell or the first `startx` milestone; those can run from a boot-provided initramfs.

MFS becomes required when MyOS moves to:

```text
block storage
      ↓
MFS
      ↓
native persistent root
      ↓
standalone MyOS installation
      ↓
self-hosting
```

The kernel-facing MFS adapter and filesystem registration remain part of [crecabar/myos](https://github.com/crecabar/myos). The filesystem specification, implementation and tooling live here.

## Related projects

- [crecabar/myos](https://github.com/crecabar/myos) — kernel, VFS, block layer and MFS kernel adapter.
- [crecabar/myos-libc](https://github.com/crecabar/myos-libc) — native C library/runtime.
- [crecabar/myos-userland](https://github.com/crecabar/myos-userland) — first-party userspace.
- [MyOS MFS epic #131](https://github.com/crecabar/myos/issues/131) — roadmap and kernel-integration tracking.

## Status

Early design/planning stage. The detailed on-disk format and implementation are intentionally not frozen yet.

## License

`myos-mfs` is licensed under the **GNU General Public License version 2 only (GPL-2.0-only)**.

See [LICENSE](LICENSE).
