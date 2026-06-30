# CluaJIT

> A native C implementation of the json library for Lua.
> Single shared library. No dependencies. No Lua files.

[![License](https://img.shields.io/badge/License-BSD__2--Clause-blue.svg)](https://opensource.org/licenses/BSD-2-Clause)
![Language](https://img.shields.io/badge/Language-C99-blue)
![Lua](https://img.shields.io/badge/Lua-5.1%20%7C%205.2%20%7C%205.3%20%7C%205.4%20%7C%20LuaJIT-blue)
![Platform](https://img.shields.io/badge/Platform-Linux%20%7C%20Android%20%7C%20macOS%20%7C%20Windows-blue)
![Branch](https://img.shields.io/badge/Branch-Research--CluaJIT--Snapshot--development-orange)

---

## What Is This

CluaJIT is the C branch of the [json](../README.md) library by TheDevinLabs.

It reimplements the entire library in a single C file — JSON encode and decode, superstring `yes`/`no` tokens, strict RFC 8259 mode, and all file and path operations — compiled into one `.so`, `.dll`, or `.dylib`.

From Lua, nothing changes:

```lua
local json = require("json")
```

---

## Files

| File | Purpose |
|---|---|
| `json.c` | The entire library in C |
| `Makefile` | Auto-detecting cross-platform build |
| `c.mod` | Module manifest |

---

## Building

### Linux

```sh
make
```

Output: `json.so`

Custom Lua path:

```sh
make LUA_INC="-I/usr/include/lua5.4" LUA_LIB="-llua5.4"
```

---

### Android — Termux

```sh
pkg install clang make lua54
make
```

Output: `json.so`

---

### macOS

```sh
make
```

Output: `json.dylib`

With Homebrew:

```sh
make LUA_INC="-I$(brew --prefix lua)/include/lua5.4"
```

---

### Windows — MinGW

```sh
make windows
```

Output: `json.dll`

MSVC:

```sh
cl /O2 /LD json.c /I C:\lua\include /link /LIBPATH:C:\lua lua54.lib /OUT:json.dll
```

---

### Build Targets

| Command | Output |
|---|---|
| `make` | Auto-detect and build |
| `make linux` | `json.so` |
| `make android` | `json.so` |
| `make macos` | `json.dylib` |
| `make windows` | `json.dll` |
| `make clean` | Remove all build outputs |
| `make info` | Print detected platform and paths |

---

## Installation

Place the output file where Lua can find it — beside your script or in the Lua path:

```
your-project/
├── main.lua
└── json.so     ← or .dll / .dylib
```

Or system-wide on Termux:

```sh
cp json.so /data/data/com.termux/files/usr/lib/lua/5.4/
```

---

## API

### `json.encode(val, opts?)`

Encodes a Lua value to a JSON string.

```lua
json.encode({ name = "Furry", active = true })
-- {"active":true,"name":"Furry"}

json.encode({ name = "Furry" }, { pretty = true })
-- {
--   "name": "Furry"
-- }

json.encode({ active = true }, { superstring = true })
-- {"active":yes}
```

| Option | Type | Default | Description |
|---|---|---|---|
| `pretty` | boolean | false | Indented output |
| `indent` | number | 2 | Spaces per indent level |
| `sort_keys` | boolean | false | Sort object keys |
| `superstring` | boolean | false | Output `yes`/`no` for booleans |

---

### `json.decode(str, opts?)`

Decodes a JSON string into a Lua value.

```lua
json.decode('{"name":"Furry","active":true}')
json.decode('{"active":yes}', { superstring = true })
json.decode('{"a":1,"a":2}', { strict = true })  -- error: duplicate key
```

| Option | Type | Default | Description |
|---|---|---|---|
| `superstring` | boolean | false | Accept `yes`/`no` as boolean tokens |
| `strict` | boolean | false | RFC 8259 strict mode |

---

### `json.null`

Sentinel value for JSON `null`. Distinct from Lua `nil`.

```lua
local t = json.decode('{"key":null}')
if t.key == json.null then
    print("null")
end
print(tostring(json.null))  -- null
```

---

### `json.validate(str)`

Validates a JSON string under strict RFC 8259 rules.

```lua
local ok = json.validate('{"a":1}')          -- true
local ok, err = json.validate('{"a":1,"a":2}') -- false, "duplicate key..."
```

---

### File I/O

```lua
json.encode_file("save.json", data, { pretty = true })
local data = json.decode_file("save.json")

json.append_file("log.ndjson", { event = "login" })
local entries = json.decode_lines("log.ndjson")
```

---

### Path and File Operations

```lua
json.exists("file.json")
json.read("file.txt")
json.write("file.txt", "content")
json.append("file.txt", "more")
json.delete("file.txt")
json.rename("old.txt", "new.txt")
json.copy("src.txt", "dst.txt")
json.size("file.txt")
json.mkdir("path/to/dir")
json.list("some/folder")
json.is_file("file.txt")
json.is_dir("folder")
json.read_lines("file.txt")
json.join("path", "to", "file.txt")
json.basename("path/to/file.txt")   -- file.txt
json.dirname("path/to/file.txt")    -- path/to
json.extension("path/to/file.txt")  -- txt
json.stem("path/to/file.txt")       -- file
```

---

## Superstring Token Reference

| Token | Decoded As |
|---|---|
| `true` | `true` |
| `false` | `false` |
| `yes` *(superstring mode)* | `true` |
| `no` *(superstring mode)* | `false` |
| `null` | `json.null` |

---

## Behaviour Reference

| Situation | Behaviour |
|---|---|
| `nan` / `inf` | Encoded as `null` |
| Circular reference | Error |
| Depth over 512 | Error |
| `nil` table value | Key skipped |
| Sequential integer keys | Array |
| Mixed / string keys | Object |
| `\uXXXX` in decode | Converted to UTF-8 |
| Surrogate pairs | Correctly decoded |
| Duplicate keys in strict mode | Error |
| Leading zeros in strict mode | Error |
| Trailing garbage | Error |

---

## Compatibility

| Platform | Compiler | Status |
|---|---|---|
| Linux x86\_64 | gcc / clang | ✅ |
| Linux arm64 | gcc / clang | ✅ |
| Android Termux arm64 | clang | ✅ |
| macOS x86\_64 / arm64 | clang | ✅ |
| Windows x86\_64 | MinGW / MSVC | ✅ |

| Lua Version | Status |
|---|---|
| 5.1 | ✅ |
| 5.2 | ✅ |
| 5.3 | ✅ |
| 5.4 | ✅ |
| LuaJIT | ✅ |

---

## License

BSD 2-Clause — maintained by **TheDevinLabs**

[tenor_gif2254601464405110964.gif](/user_uploads/92126/dVQbjvFQ2T-XTfpJBMx1_yoE/tenor_gif2254601464405110964.gif)

