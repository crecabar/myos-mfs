# MFS Build System Design

> **Status:** adopted build conventions; implementation pending.

## Purpose

Provide one understandable GNU Make build for the host-side MFS library, utilities, and tests, while preserving a kernel-compatible, freestanding core for later linkage into `kernel.elf`.

This document describes required behavior. It does not claim a Makefile, source objects, library archive, or runnable utilities currently exist.

## Toolchain

- Use **C17** as the initial language standard.
- Use **Clang** for the first macOS host build. Permit overriding `CC` and `AR` for other hosts.
- Supply warning flags suitable for development (at least `-Wall -Wextra -Wpedantic`), without hiding warnings in individual modules.
- Build public-header consumers with a declared include root equivalent to `-Iinclude`; all MFS headers use canonical `#include <mfs/...>` syntax.
- Produce dependencies with `-MMD -MP` for relevant C object rules, including header-triggered incremental rebuilds.
- Support parallel GNU Make builds and avoid recipes that race while creating output directories.

Host binaries on macOS are Mach-O. A future MyOS build will use its own cross/freestanding toolchain and ELF link; the host toolchain must not force kernel flags onto host utilities.

## Dependency boundaries

| Component | Permitted dependencies | Not permitted |
| --- | --- | --- |
| `lib/` portable core | freestanding C types/definitions, explicitly provided memory operations, MFS contracts | POSIX file APIs, `stdio`, implicit heap allocation, platform-private interfaces |
| `platform/posix/` | host libc and POSIX services | MyOS kernel internals |
| `platform/myos/` | MyOS kernel/block interfaces through explicit adapter boundaries | host POSIX system calls, userspace libc assumptions |
| `tools/` | hosted standard C library, POSIX backend as required | private MyOS kernel implementation |
| `tests/` | host test support and intentional interfaces under test | dependencies leaking into production builds |

The freestanding core must not assume `malloc()`, `printf()`, `fopen()`, `errno`, or a filesystem on the host. It may eventually request memory or I/O through deliberately defined caller-supplied operations, documented independently of either platform.

Using common C definitions such as `<stdint.h>`, `<stddef.h>`, `<stdbool.h>`, and `<limits.h>` is consistent with the portable core. More library facilities require explicit review of their availability and linkage in the MyOS kernel environment. MyOS's userspace libc is **not** the default library linked into the kernel.

## Build layout

```text
Makefile
config.mk                # tool and flag defaults, when introduced
mk/                      # optional per-target make fragments as the project grows
include/mfs/             # public headers
lib/                     # freestanding-compatible core
platform/posix/          # hosted I/O backend
platform/myos/           # MyOS integration
tools/                   # standalone host utilities
tests/                   # host tests and fixtures
build/                   # generated artifacts; never committed
```

Start with a small top-level Makefile and only split it into `mk/*.mk` when doing so gives each fragment a clear responsibility. Do not invent empty make fragments ahead of implementation.

Suggested eventual generated outputs:

```text
build/
  obj/                   # per-target objects and generated dependencies
  lib/libmfs.a
  bin/mkfs.mfs
  bin/fsck.mfs
  bin/mfsdump
  tests/                 # test executables (not user-managed image fixtures)
```

Independent target configurations must not share incompatible object paths. The portable archive must not accidentally contain POSIX adapter objects.

## Target contract

| Target | Meaning |
| --- | --- |
| `all` | Build all **currently implemented** default host artifacts |
| `lib` | Build the portable archive once sources exist |
| `tools` | Build available host tools |
| `test` | Build and execute available tests, failing on any test failure |
| `check` | Run available code-quality checks and tests |
| `clean` | Remove owned build artifacts only |
| `help` | Show supported targets |

Do not create fictitious successful targets for non-existent components. A target that is not implemented must either be omitted temporarily or fail with an explicit explanation, especially `test` and `check`. No-op success must not be confused with a test pass.

The earliest real smoke test should compile and run a tiny hosted executable which includes `<mfs/version.h>` and prints `MFS_VERSION_STRING`. That program belongs to the tools/tests side, not to `lib/`, because it calls `printf()`.

## Safety and cleanup

- `clean` removes only build-owned output paths (preferably `build/`) and never touches user-provided image files, physical devices, source fixtures, or repository directories.
- Executable tools that modify images must require a clear target; compiling a utility must never modify an image.
- Generated `.d` and compiler outputs stay under ignored build directories.
- Input paths to destructive operations must not be inferred from build variables.

## First implementation checkpoint

1. Add `Makefile` (and `config.mk` only if it improves clarity).
2. Add a tiny hosted version-printing smoke test using `#include <mfs/version.h>`.
3. Build with Clang/C17 and explicit `-Iinclude`.
4. Verify clean build, incremental no-op rebuild, rebuild after the public header changes, and `make -j4` behavior.
5. Verify `make clean` leaves all source documents and test images untouched.
6. Only then add a portable C object and library archive from actual sources.

The checkpoint is successful when each claimed target does real work and compilation remains cleanly separated from filesystem format design.

## Relationship to design specifications

- [Architecture](architecture.md) owns platform/core boundaries.
- [Include policy](include-policy.md) owns canonical header names.
- [Initial format decisions](format-design-decisions.md) keeps the on-disk ABI deliberately unfrozen.
- [Development plan](development-plan.md) defines filesystem functionality milestones.
