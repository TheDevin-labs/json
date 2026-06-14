# JSON

A most wondrous, lightweight, and local library, fashioned in the native tongue of Lua for the encoding, decoding, stewardship of JSON metadata, file operations, and the great reporting of errors.

[![License](https://img.shields.io/badge/License-BSD__2--Clause-blue.svg)](https://opensource.org/licenses/BSD-2-Clause)
![Language](https://img.shields.io/badge/Language-Lua-blue)

---

> [!NOTE]
> Take heed, gentle practitioner! This library abideth wholly in isolation, utterly severed from the global web. Wherefore, whenever a new craft or amendment is brought forth, thou art required to fetch and install it locally upon thy machine. Be not troubled, for no future revision shall break or confound the work thou hast already established.

## Notable Attributes

* **One True Scroll:** All power dwelleth within `json.lua` alone. Superstring and Great Errors are built in — no separate require needed to unlock them.
* **On File Stewardship:** Containeth a companion scroll (`PathFiles.lua`) to navigate and govern thy file systems with great ease.
* **Swift & Unburdened:** Wrought in pure Lua, demanding no external alliances nor strange dependencies.
* **Secure & Recluse:** Absent of all internet commerce, thereby ensuring the absolute privacy of thy data.

---

## The Scrolls of This Library

| Scroll | Role |
|---|---|
| `json.lua` | The one true core — encoding, decoding, strict mode, superstring, great errors, and file I/O |
| `PathFiles.lua` | File and directory stewardship |
| `superstring.lua` | Thin companion — `yes`/`no` mode pre-enabled for convenience |
| `greaterror.lua` | Thin companion — error display controls for convenience |
| `lua.mod` | The module manifest |

> `superstring.lua` and `greaterror.lua` are **companion scrolls**, not separate engines. Their powers already live inside `json.lua`. They exist purely so thou needst not pass options by hand every time.

---

## How One May Import This Craft

**1. Deposit the Files** into a folder inside thy project, perchance named `json`:

```
your-project/
└── json/
    ├── json.lua
    ├── PathFiles.lua
    ├── superstring.lua
    ├── greaterror.lua
    └── lua.mod
```

**2. Invoke the Core** at the summit of thy script:

```lua
local json = require("json/json")
```

That single line unlocketh everything — encoding, decoding, strict mode, superstring tokens, great errors, and file I/O.

---

## Making This Library a Standard — Call It from Anywhere

If thou desirest to call `json` from any script in thy project without writing `require("json/json")` each time, thou mayest install it as a standard library in one of two ways:

### Way the First — The `package.path` Declaration

At the very top of thy main entry script, extend Lua's search path:

```lua
package.path = package.path .. ";./json/?.lua"

local json = require("json")
```

From this point onward, any script that runs beneath thy main script may call `require("json")` directly, without a folder prefix.

### Way the Second — The Global Declaration

If thou desirest `json` to be available everywhere without even calling `require`, place this at the very top of thy main entry script:

```lua
package.path = package.path .. ";./json/?.lua"
json = require("json")
```

Note the absence of `local`. This maketh `json` a global name, visible to every script and every module that runs thereafter, as if it were a built-in part of the language itself.

```lua
local data = json.decode('{"guild":"TheDevinLabs"}')
print(data.guild)
```

### Way the Third — Installing to Lua's System Path (Termux / Linux)

If thou art on Termux or Linux and desirest the library to be available system-wide across all projects:

```sh
cp json.lua PathFiles.lua superstring.lua greaterror.lua /data/data/com.termux/files/usr/share/lua/5.4/
```

Or on a standard Linux machine:

```sh
sudo cp json.lua PathFiles.lua superstring.lua greaterror.lua /usr/local/share/lua/5.4/
```

After this, any Lua script on the machine may call:

```lua
local json = require("json")
```

With no folder, no path extension, no ceremony whatsoever.

---

## Examples of the Craft

### 1. Translating a Lua Table into JSON Text (Encoding)

```lua
local json = require("json/json")

local character_profile = {
    username       = "CoolyDucks",
    guild          = "TheDevinLabs",
    is_active      = true,
    projects_count = 3,
    languages      = { "Lua", "JSON", "Markdown" }
}

local json_parchment = json.encode(character_profile)
print(json_parchment)
```

For a more readable parchment:

```lua
local pretty = json.encode(character_profile, { pretty = true, sort_keys = true })
print(pretty)
```

### 2. Translating JSON Text Back into Lua (Decoding)

```lua
local json = require("json/json")

local raw_scroll = '{"guild": "TheDevinLabs", "status": "Active"}'

local data_table = json.decode(raw_scroll)
print("The Guild Name is: " .. data_table.guild)
```

### 3. Reading and Writing JSON Files

```lua
local profile = { name = "CoolyDucks", level = 42 }

json.encode_file("save.json", profile, { pretty = true })

local loaded = json.decode_file("save.json")
print(loaded.name)
```

For a running journal of events:

```lua
json.append_file("log.ndjson", { event = "login",  user = "CoolyDucks" })
json.append_file("log.ndjson", { event = "logout", user = "CoolyDucks" })

local entries = json.decode_lines("log.ndjson")
for _, entry in ipairs(entries) do
    print(entry.event, entry.user)
end
```

---

## Superstring — The Yes and No Tongue

Superstring mode is built directly into `json.lua`. Pass `{ superstring = true }` to any encode or decode call:

```lua
json.encode({ active = true, banned = false }, { superstring = true })
-- {"active":yes,"banned":no}

json.decode('{"active":yes,"banned":no}', { superstring = true })
-- { active = true, banned = false }

json.decode('[yes, no, true, false]', { superstring = true })
-- { true, false, true, false }
```

Or use the `superstring.lua` companion, which hath `{ superstring = true }` pre-applied so thou needst not write it each time:

```lua
local superstring = require("json/superstring")

superstring.encode({ active = true })
-- {"active":yes}

superstring.decode('{"active":yes}')
-- { active = true }
```

### Superstring Token Reference

| Token | Decoded As |
|---|---|
| `true` | `true` |
| `false` | `false` |
| `yes` | `true` |
| `no` | `false` |
| `null` | `json.null` |

`null` always taketh precedence over `no` — the parser checketh `null` first, so there is no ambiguity between the two.

---

## Strict Mode — RFC 8259 Fidelity

Strict mode is built directly into `json.lua`. Pass `{ strict = true }`:

```lua
json.decode('{"a":1,"a":2}', { strict = true })
-- Great Error: duplicate key "a" not allowed (RFC 8259)

json.decode('01', { strict = true })
-- Great Error: leading zeros not allowed (RFC 8259)

json.encode({ n = 0/0 }, { strict = true })
-- Great Error: non-finite number is not allowed in strict mode
```

Both modes may be combined:

```lua
json.decode('{"enabled":yes,"count":42}', { strict = true, superstring = true })
-- { enabled = true, count = 42 }
```

### Validation

```lua
local ok = json.validate('{"a":1}')
-- true

local ok, err = json.validate('{"a":1,"a":2}')
-- false, "duplicate key..."

json.is_valid_utf8("héllo")   -- true
json.is_valid_utf8("\xFF")    -- false
```

---

## The Great Error

When things go awry, `json.lua` speaketh loudly. Instead of a bare `nil` or a plain Lua error, it printeth to stderr in full colour with source, message, detail, hint, and stack trace:

```
╔══ ERROR ══════════════════════════════════════╗
║  source  : json.decode
║  message : unexpected character "}" at position 13
║  detail  : got: "}"
║  hint    : check for invalid characters or unquoted strings
║  trace   :
║    at myscript.lua:10
╚═══════════════════════════════════════════════╝
```

This requireth nothing from thee — it simply happeneth whenever a fault is detected.

### Configuring the Great Error from json.lua

```lua
json.error_set_color(false)
json.error_set_trace(false)
json.error_set_level(json.WARNING)
json.error_reset()
```

`error_set_color(false)` — disableth ANSI colour codes, useful when writing to a plain log file.
`error_set_trace(false)` — disableth the stack trace lines.
`error_set_level(level)` — setteth the minimum severity that will be displayed. Levels are `json.FATAL`, `json.ERROR`, `json.WARNING`, `json.INFO`.
`error_reset()` — restoreth all settings to their defaults.

### Installing a Custom Handler

If thou desirest to redirect errors to thy own logging system:

```lua
json.error_set_handler(function(level, source, message, detail, hint, formatted)
    my_logger.write(level.label .. " [" .. source .. "] " .. message)
end)
```

When a handler is installed, the default stderr output is suppressed entirely.

### Using the greaterror.lua Companion

If thou preferest to configure error display through a dedicated name:

```lua
local greaterror = require("json/greaterror")

greaterror.set_color(false)
greaterror.set_trace(false)
greaterror.set_level(greaterror.WARNING)
greaterror.reset()

greaterror.set_handler(function(level, source, message, detail, hint, formatted)
    my_logger.write(formatted)
end)
```

This is identical to calling `json.error_*` — the companion merely giveth it a different name for readability.

### Severity Levels

| Level | Behaviour |
|---|---|
| `json.FATAL` | Print Great Error + `os.exit(1)` |
| `json.ERROR` | Print Great Error + raise Lua error |
| `json.WARNING` | Print only, never raise |
| `json.INFO` | Print only, never raise |

---

## The PathFiles Module

`PathFiles.lua` governeth all file and directory dealings.

```lua
local PathFiles = require("json/PathFiles")
```

### Reading

```lua
local content = PathFiles.read("file.txt")
local lines   = PathFiles.read_lines("file.txt")
local bytes   = PathFiles.size("file.txt")
```

### Writing

```lua
PathFiles.write("file.txt", "hello world")
PathFiles.append("file.txt", "\nmore words")
```

### Checking

```lua
PathFiles.exists("file.txt")
PathFiles.is_file("file.txt")
PathFiles.is_dir("some/folder")
```

### Moving and Copying

```lua
PathFiles.copy("source.txt", "destination.txt")
PathFiles.rename("old.txt", "new.txt")
PathFiles.delete("unwanted.txt")
```

### Directories

```lua
PathFiles.mkdir("path/to/new/dir")
local entries = PathFiles.list("some/folder")
```

### Path Utilities

```lua
PathFiles.join("path", "to", "file.txt")   -- path/to/file.txt
PathFiles.basename("path/to/file.txt")     -- file.txt
PathFiles.dirname("path/to/file.txt")      -- path/to
PathFiles.extension("path/to/file.txt")    -- txt
PathFiles.stem("path/to/file.txt")         -- file
```

---

## Behaviour Reference

| Situation | Behaviour |
|---|---|
| `nan` / `inf` in encode | Encoded as `null` — Great Error in strict mode |
| Circular reference | Great Error thrown |
| Depth over 512 | Great Error thrown |
| `nil` value in table | Key skipped silently |
| Mixed table (int + string keys) | Encoded as object |
| Sequential integer keys | Encoded as array |
| UTF-8 strings | Passed through as-is |
| `\uXXXX` escape in decode | Converted to UTF-8 |
| Surrogate pairs `\uD800\uDC00` | Correctly decoded to UTF-8 |
| JSON `null` decoded | Returns `json.null` sentinel |
| Trailing garbage | Great Error thrown |
| Duplicate keys in strict mode | Great Error thrown |
| Leading zeros in strict mode | Great Error thrown |
| `yes` / `no` in superstring mode | Decoded as `true` / `false` |

---

## Compatibility

| Platform | Status |
|---|---|
| Linux x86\_64 | ✅ |
| Linux arm64 / Android Termux | ✅ |
| macOS | ✅ |
| Windows | ✅ |
| Lua 5.1 | ✅ |
| Lua 5.2 | ✅ |
| Lua 5.3 | ✅ |
| Lua 5.4 | ✅ |

---

## The Module Manifest

```
module json

count as = [library]

include json.lua
include PathFiles.lua
include superstring.lua
include greaterror.lua
```

---

## License & Covenant of Fair Use

This work is bound by the terms of the BSD 2-Clause Simplified License.

* **An Open Source:** Thou art granted full liberty to employ, alter, and distribute these codes as seest fit.
* **A Solemn Disclaimer:** Whilst thou mayest use this craft for any noble purpose, thou shalt not covet nor steal the name of this project, nor falsely proclaim it as thine own invention.

Maintained with due diligence by the guild of **TheDevinLabs**
