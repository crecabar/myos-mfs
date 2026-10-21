# MFS Initial Format Design Decisions

> **Status:** design baseline for review; byte-level format **not frozen**.

## Objective

Define what must be decided before `mkfs.mfs` can create the first experimental image and `mfsdump` can recognize it. Distinguish agreed architectural requirements from proposals and unresolved wire-format details.

## Agreed constraints

- MFS is a volume format independent of partition maps, controllers, the MyOS kernel, and the host executable format.
- On-disk layout is an explicit byte-level ABI; C structure padding and native host endianness cannot define it.
- All fields are bounded and validated before arithmetic and access.
- Disk-format compatibility is separate from the implementation version in `<mfs/version.h>`.
- Metadata journaling and recovery are required for a stable writable filesystem, even if the first experimental image is read-only.
- The first tools operate on disposable regular-file images; raw-device writes are out of scope.
- No initial disk image is deemed stable or backward-compatible before an explicit format freeze.

## Proposed starting choices (not yet normative)

| Topic | Proposal | Reason | Remains to decide |
| --- | --- | --- | --- |
| Filesystem logical block | 4096 bytes | Simple initial accounting and alignment | Whether format v0 allows other sizes |
| Byte order | Little-endian for multi-byte integers | Natural for the first x86_64 target, explicit on any host | Exact encoding helper contract |
| Addressing | Volume-relative, zero-based logical block numbers | Decouples partitions and devices | Maximum volume/block-address width |
| Initial superblock | One designated early logical block, with block 0 reserved | Easy to inspect and leaves room for boot/other metadata | Exact block location, backup policy |
| Identity | Fixed magic plus format version | Explicit rejection of unrelated media | Exact magic bytes and version fields |
| Integrity | Checksum on critical metadata | Detects accidental corruption | Algorithm, coverage, seed and update rules |
| Feature negotiation | Required/optional feature classes | Controlled evolution of readers and writers | Bit allocations and compatibility matrix |
| Journal | Metadata journal planned from the layout stage | Avoids incompatible later redesign | Placement, capacity, records, commit protocol |

These proposals must not be copied into implementation as promises until the on-disk specification resolves their exact wire representation.

## Superblock design questions

The first complete specification must identify the superblock by absolute **volume-relative byte offset**, not just a C struct name. It must define the following for each stored field: offset, size, endianness, allowed range, required value, and checksum inclusion.

Candidate information includes:

- magic and on-disk format revision;
- header/structure length and block size;
- volume length and UUID;
- region descriptors for allocation, inode and data metadata;
- journal boundaries and format state;
- compatibility flags;
- integrity checksum and reserved-byte policy.

A reserved field is not a license to accept arbitrary contents; readers must have explicit rules for future extension and unknown values.

## Validation requirements

An image recognizer must:

1. Read only after checking backend capacity and requested offset/size bounds.
2. Reject short reads, wrong magic, unsupported required version/features, impossible dimensions, and arithmetic overflow.
3. Verify the superblock checksum once its algorithm is specified.
4. Check that every referenced region remains inside the selected volume and does not overlap forbidden regions.
5. Report precisely which checks were performed. Superblock validation alone is **not** a successful full filesystem check.

Tests must include valid reopen, truncated images, changed magic, invalid version, corrupted checksum, out-of-range region pointers, and integer overflow cases when the corresponding fields exist.

## Compatibility and recovery boundary

The first image may contain only enough metadata to be recognized. It must not advertise a clean, writable, crash-recoverable filesystem if transaction and recovery support does not exist. Tool output must identify an experimental image appropriately.

Before any writable MFS release, the project must specify and test durability guarantees, journal ordering, transaction commit semantics, replay behavior, and incomplete-operation handling. The exact scheme remains a design task.

## Follow-on documents

1. `docs/on-disk-format.md` — authoritative offsets, wire types, layout, compatibility and validation rules once decisions are made.
2. `docs/architecture.md` — platform-neutral I/O and system ownership.
3. `docs/development-plan.md` — milestone acceptance criteria.

The next design slice should turn the superblock proposal into a precise byte-offset table, include at least one reference encoding fixture, and review how the minimum image remains extensible.
