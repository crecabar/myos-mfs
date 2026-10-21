# MFS Header and Include Policy

> **Status:** adopted project convention.

## Purpose

MFS uses canonical angle-bracket includes **from the first source file**. The path communicates API ownership, remains stable across changes to the physical layout of source directories, and must be understood consistently by the host build, MyOS build, IDE tooling, and tests.

## Rules

1. All MFS-owned headers are included using the angle-bracket form: `#include <mfs/...>`.
2. The public include root is `include/`. A public header such as `include/mfs/version.h` is consumed as `#include <mfs/version.h>`.
3. When an internal header is required, give it an explicit private include root and namespace rather than using `#include "header.h"`, `../` traversal, or `-I.`. The precise private root will be established with the first implementation slice.
4. Platform-specific interfaces must remain in their owning platform module; the platform-neutral core must not include a POSIX or MyOS-specific header.
5. Standard C headers retain their conventional `<stddef.h>`, `<stdint.h>`, and related names. External library headers also use angle brackets.
6. Build targets expose only the include roots they require. Production targets must not resolve private test headers.
7. Do not add compatibility forwarding headers to preserve obsolete include paths.
8. A header move does not authorize modifications to the persistent disk format or public API contracts.

## Examples

Correct:

```c
#include <mfs/version.h>
#include <stdint.h>
```

Incorrect:

```c
#include "version.h"
#include "../include/mfs/version.h"
#include "mfs/version.h"
```

## Compilation and IDE integration

The initial host build must pass `-Iinclude` (or the equivalent target-scoped include directory configuration). A MyOS integration target must likewise provide the public include root deliberately; it must not rely on working-directory-relative includes. Export compile commands or configure the IDE target rather than changing source includes to silence editor warnings.

## Scope

This rule applies to portable library code, host tooling, MyOS integration code, and tests. It is an intentional MFS project policy, stricter than legacy local-header conventions elsewhere. No source files need to be migrated yet.
