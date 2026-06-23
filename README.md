# CluaJIT

> Native C acceleration layer for the [json](../README.md) library by TheDevinLabs.
> Branch: `Research-CluaJIT-Snapshot-development`

[![License](https://img.shields.io/badge/License-BSD__2--Clause-blue.svg)](https://opensource.org/licenses/BSD-2-Clause)
![Language](https://img.shields.io/badge/Language-C99-blue)
![Lua](https://img.shields.io/badge/Lua-5.1%20%7C%205.2%20%7C%205.3%20%7C%205.4%20%7C%20LuaJIT-blue)
![Platform](https://img.shields.io/badge/Platform-Linux%20%7C%20Android%20%7C%20macOS%20%7C%20Windows-blue)
![Status](https://img.shields.io/badge/Status-Research%20Snapshot-orange)

---

> [!WARNING]
> This is a research snapshot branch. It is not part of the stable release. The pure Lua fallback in `json.lua` remains fully intact — if CluaJIT cannot be built or loaded on your platform, nothing breaks.

---

## Overview

CluaJIT replaces the inner encode and decode loops of the json library with compiled native C. It is not a separate tool — it is a drop-in acceleration layer. When the compiled modules are present, `json.lua` loads them automatically. When they are absent, the pure Lua engine runs unchanged.

```
require("json")
      │
      ▼
tries require("cluajit")  ──found──▶  C engine  (json.c / superstring.c / PathFiles.c)
      │
   not found
      │
      ▼
  pure Lua engine  (always works)
```

---

## File Structure

```
Research-CluaJIT-Snapshot-development/
├── cluajit.h         Shared header — Buffer, Parser, constants, declarations
├── json.c            JSON encode + decode in C
├── superstring.c     Superstring encode + decode in C (yes/no tokens built in)
├── PathFiles.c       File and directory operations in C
├── Makefile          Auto-detecting cross-platform build system
├── c.mod             Module manifest for this branch
└── README-CluaJIT.md This file
```

---

## Modules

### `json.c` → `json.so` / `json.dll` / `json.dylib`

Accelerates `json.encode` and `json.decode`. Supports all options: `pretty`, `indent`, `sort_keys`, `strict`. Handles full UTF-8, `\uXXXX` escapes, surrogate pairs, `nan`/`inf` → `null`, max depth 512.

### `superstring.c` → `superstring.so` / `superstring.dll` / `superstring.dylib`

Accelerates `superstring.encode` and `superstring.decode`. Superstring mode (`yes`/`no` tokens) is always active. Supports `strict`, `pretty`, `indent`, `sort_keys`.

### `PathFiles.c` → `pathfiles.so` / `pathfiles.dll` / `pathfiles.dylib`

Replaces the shell-based `PathFiles.lua` with pure C system calls. No `io.popen`, no `os.execute`. Full native directory listing, recursive mkdir, stat-based file/dir detection, binary-safe read and write.

### `cluajit.h`

Shared header included by all three modules. Defines `Buffer`, `Parser`, constants (`CLUAJIT_MAX_DEPTH`, `CLUAJIT_BUF_INIT`), Lua version compatibility macros, and all shared function declarations.

---

## Requirements

| Requirement | Notes |
|---|---|
| C compiler | gcc, clang, tcc, or MSVC — any C99-capable compiler |
| Lua headers | `lua.h` and `lauxlib.h` for your installed Lua version |
| GNU Make | Or any compatible make |

---

## Building

### Linux

```sh
make
```

Produces `json.so`, `superstring.so`, `pathfiles.so`.

Custom Lua header path:

```sh
make LUA_INC="-I/usr/include/lua5.4" LUA_LIB="-llua5.4"
```

### Android — Termux (arm64)

```sh
pkg install clang make lua54
make
```

The Makefile detects `aarch64` automatically and uses Termux paths. Produces `.so` files.

### macOS

```sh
make
```

Produces `json.dylib`, `superstring.dylib`, `pathfiles.dylib`.

With Homebrew:

```sh
make LUA_INC="-I$(brew --prefix lua)/include/lua5.4"
```

### Windows — MinGW

```sh
make windows
```

Cross-compiles using `x86_64-w64-mingw32-gcc`. Produces `json.dll`, `superstring.dll`, `pathfiles.dll`.

Native MSVC:

```sh
cl /O2 /LD json.c /I C:\lua\include /link /LIBPATH:C:\lua lua54.lib /OUT:json.dll
cl /O2 /LD superstring.c /I C:\lua\include /link /LIBPATH:C:\lua lua54.lib /OUT:superstring.dll
cl /O2 /LD PathFiles.c /I C:\lua\include /link /LIBPATH:C:\lua lua54.lib /OUT:pathfiles.dll
```

---

## Build Targets

| Command | Action |
|---|---|
| `make` | Auto-detect platform and build all three modules |
| `make linux` | Force Linux build |
| `make android` | Force Android / Termux build |
| `make macos` | Force macOS build |
| `make windows` | Force Windows build via MinGW |
| `make clean` | Remove all `.so`, `.dylib`, `.dll`, `.o` files |
| `make info` | Print detected platform, arch, compiler, and paths |

---

## Installation

After building, place the output files beside `json.lua`:

```
your-project/
└── json/
    ├── json.lua
    ├── PathFiles.lua
    ├── superstring.lua
    ├── greaterror.lua
    ├── json.so            ← or .dll / .dylib
    ├── superstring.so
    ├── pathfiles.so
    ├── cluajit.h
    ├── json.c
    ├── superstring.c
    ├── PathFiles.c
    ├── Makefile
    └── c.mod
```

`json.lua` finds and loads the C modules automatically on the next `require("json")`.

---

## Checking the Active Engine

```lua
local json = require("json")

if json.has_cluajit() then
    print("C engine active — CluaJIT")
else
    print("Pure Lua engine active")
end
```

---

## Feature Coverage

| Feature | `json.c` | `superstring.c` | `PathFiles.c` |
|---|---|---|---|
| Encode strings, numbers, booleans | ✅ | ✅ | — |
| Encode arrays and objects | ✅ | ✅ | — |
| Encode pretty print + indent | ✅ | ✅ | — |
| Encode sort keys | ✅ | ✅ | — |
| Encode `nan`/`inf` → `null` | ✅ | ✅ | — |
| Encode `yes`/`no` superstring tokens | — | ✅ | — |
| Decode strings + all escapes | ✅ | ✅ | — |
| Decode `\uXXXX` unicode | ✅ | ✅ | — |
| Decode surrogate pairs | ✅ | ✅ | — |
| Decode `yes`/`no` tokens | — | ✅ | — |
| Decode strict mode (RFC 8259) | ✅ | ✅ | — |
| Decode duplicate key rejection | ✅ | ✅ | — |
| Decode leading zero rejection | ✅ | ✅ | — |
| Max depth 512 | ✅ | ✅ | — |
| Read / write files | — | — | ✅ |
| Append files | — | — | ✅ |
| Read lines | — | — | ✅ |
| Delete / rename / copy | — | — | ✅ |
| Recursive mkdir | — | — | ✅ |
| Directory listing | — | — | ✅ |
| is_file / is_dir / size | — | — | ✅ |
| Path utilities (join, stem, ext) | — | — | ✅ |

---

## Lua Version Compatibility

CluaJIT uses preprocessor guards to support every Lua C API version from the same source:

```c
#if LUA_VERSION_NUM >= 502
    luaL_newlib(L, lib);
#else
    luaL_register(L, "name", lib);
#endif

#if LUA_VERSION_NUM >= 503
    if (lua_isinteger(L, idx)) { ... }
#endif
```

| Version | Status |
|---|---|
| Lua 5.1 | ✅ |
| Lua 5.2 | ✅ |
| Lua 5.3 | ✅ |
| Lua 5.4 | ✅ |
| LuaJIT | ✅ |

---

## Platform Compatibility

| Platform | Compiler | Status |
|---|---|---|
| Linux x86\_64 | gcc / clang | ✅ |
| Linux arm64 | gcc / clang | ✅ |
| Android Termux arm64 | clang | ✅ |
| macOS x86\_64 | clang | ✅ |
| macOS arm64 (Apple Silicon) | clang | ✅ |
| Windows x86\_64 | MinGW gcc | ✅ |
| Windows x86\_64 | MSVC | ✅ |

---

## The c.mod Manifest

```
module cluajit

count as = [library]

lang = C99
abi  = lua-c-api

include cluajit.h
include json.c
include superstring.c
include PathFiles.c
include Makefile

targets = [linux, android, macos, windows]
output  = [json.so, superstring.so, pathfiles.so]
output  = [json.dylib, superstring.dylib, pathfiles.dylib]
output  = [json.dll, superstring.dll, pathfiles.dll]

depends on = json
```

---

## Branch Relationship

```
main
└── json.lua          Pure Lua — always works
    PathFiles.lua
    superstring.lua
    greaterror.lua
    lua.mod

Research-CluaJIT-Snapshot-development
└── json.lua          Updated — loads C modules if present
    cluajit.h         Shared C header
    json.c            C encode + decode
    superstring.c     C superstring encode + decode
    PathFiles.c       C file operations
    Makefile
    c.mod
```

---

## License

BSD 2-Clause — see [LICENSE](../LICENSE).
Maintained by **TheDevinLabs**.
