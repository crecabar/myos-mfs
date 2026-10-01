# MFS on-disk format

> **Status:** design placeholder — the MFS on-disk format is not frozen yet.

This document will define the persistent on-disk format of **MFS (MyOS File System)**.

The format must remain independent from private MyOS kernel implementation details so that the same filesystem structures can be created, inspected, validated and recovered by host tools on macOS and Linux as well as by MyOS itself.

## Scope

The specification will eventually define, at minimum:

- filesystem identification and versioning;
- block size and fundamental layout;
- superblock representation;
- inode representation;
- directory representation;
- free-space and block allocation metadata;
- data extent/block mapping;
- journal location and transaction records;
- checksums;
- feature flags and compatibility rules;
- persistence and write-ordering requirements;
- crash-recovery semantics;
- corruption-detection rules;
- endianness and integer-width requirements.

## Design constraints

The on-disk format must be:

- explicitly versioned;
- deterministic and documented;
- independent of compiler structure padding;
- expressed using fixed-width integer types;
- portable across supported host tooling and MyOS;
- robust against malformed or partially written metadata;
- suitable for transactional metadata journaling;
- testable using filesystem images without booting MyOS.

No C structure should be treated as an on-disk ABI merely by writing its in-memory representation directly to disk unless the representation has been explicitly defined and validated as part of this specification.

## Compatibility

Compatibility and feature-negotiation rules will be defined before the first format version is declared stable.

Until that point, filesystem images produced by development builds must be considered disposable.

## Related components

The implementation corresponding to this specification will live primarily under:

```text
include/mfs/
lib/format/
lib/inode/
lib/directory/
lib/allocation/
lib/journal/
lib/recovery/
```

Platform-specific I/O and operating-system integration belong under `platform/`, while standalone utilities belong under `tools/`.
