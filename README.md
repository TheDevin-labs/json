# JSON

A most wondrous, lightweight, and local library, fashioned in the native tongue of Lua for the encoding, decoding, stewardship of JSON metadata, file operations, and the great reporting of errors.

[![License](https://img.shields.io/badge/License-BSD__2--Clause-blue.svg)](https://opensource.org/licenses/BSD-2-Clause)
![Language](https://img.shields.io/badge/Language-Lua-blue)

---

> [!NOTE]
> Take heed, gentle practitioner! This library abideth wholly in isolation, utterly severed from the global web. Wherefore, whenever a new craft or amendment is brought forth, thou art required to fetch and install it locally upon thy machine. Be not troubled, for no future revision shall break or confound the work thou hast already established.

## Notable Attributes

* **On File Stewardship:** Containeth a dedicated module (`PathFiles.lua`) to navigate and govern thy file systems with great ease.
* **On Strict Fidelity:** Containeth a superstring module (`superstring.lua`) that enforceth RFC 8259 compliance and extendeth the tongue of JSON with the words `yes` and `no`.
* **On Great Errors:** Containeth an error module (`greaterror.lua`) that speaketh loudly and clearly when things go awry, adorning each fault with colour, trace, and counsel.
* **Swift & Unburdened:** Wrought in pure Lua, demanding no external alliances nor strange dependencies.
* **Secure & Recluse:** Absent of all internet commerce, thereby ensuring the absolute privacy of thy data.

---

## The Scrolls of This Library

| Scroll | Purpose |
|---|---|
| `json.lua` | The core — encoding, decoding, and file I/O |
| `PathFiles.lua` | File and directory stewardship |
| `superstring.lua` | RFC 8259 strictness and the `yes`/`no` tongue |
| `greaterror.lua` | The Great Error — loud, coloured, and wise |
| `lua.mod` | The module manifest |

---

## How One May Import This Craft

Follow these precise steps to graft this library into thine own Lua endeavor:

**1. Deposit the Files:** Convey all scrolls into a designated folder inside thy project directory, perchance named `json`:

```
your-project/
└── json/
    ├── json.lua
    ├── PathFiles.lua
    ├── superstring.lua
    ├── greaterror.lua
    └── lua.mod
```

**2. Invoke the Code:** Bind the library at the very summit of thy script:

```lua
local json        = require("json/json")
local PathFiles   = require("json/PathFiles")
local superstring = require("json/superstring")
local greaterror  = require("json/greaterror")
```

---

## Examples of the Craft

### 1. Translating a Lua Table into JSON Text (Encoding)

Shouldst thou desire to convert thy native Lua structures into a clean JSON parchment, employ this method:

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

For a more readable parchment, thou mayest invoke the pretty option:

```lua
local json_parchment = json.encode(character_profile, { pretty = true, sort_keys = true })
print(json_parchment)
```

### 2. Translating JSON Text Back into Lua (Decoding)

When receiving raw JSON text from a ledger or file, thou canst easily restore it back into a proper Lua table:

```lua
local json = require("json/json")

local raw_scroll = '{"guild": "TheDevinLabs", "status": "Active"}'

local data_table = json.decode(raw_scroll)
print("The Guild Name is: " .. data_table.guild)
```

### 3. Reading and Writing JSON Files

When thy data must be kept in a file for safekeeping:

```lua
local json = require("json/json")

local profile = { name = "CoolyDucks", level = 42 }

json.encode_file("save.json", profile, { pretty = true })

local loaded = json.decode_file("save.json")
print(loaded.name)
```

For appending many records line by line (a log or journal):

```lua
json.append_file("log.ndjson", { event = "login",  user = "CoolyDucks" })
json.append_file("log.ndjson", { event = "logout", user = "CoolyDucks" })

local entries = json.decode_lines("log.ndjson")
for _, entry in ipairs(entries) do
    print(entry.event, entry.user)
end
```

---

## The PathFiles Module

`PathFiles.lua` governeth all file and directory dealings without need of json itself.

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
PathFiles.exists("file.txt")      -- true or false
PathFiles.is_file("file.txt")     -- true or false
PathFiles.is_dir("some/folder")   -- true or false
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

## The Superstring Module

`superstring.lua` extendeth json with RFC 8259 strictness and the power of `yes` and `no` as boolean tokens.

```lua
local superstring = require("json/superstring")
```

### Encoding with Superstring

```lua
superstring.encode({ active = true, banned = false }, { superstring = true })
-- {"active":yes,"banned":no}

superstring.encode({ active = true }, { superstring = true, pretty = true })
-- {
--   "active": yes
-- }
```

### Decoding with Superstring

```lua
superstring.decode('{"active":yes,"banned":no}', { superstring = true })
-- { active = true, banned = false }

superstring.decode('[yes, no, true, false]', { superstring = true })
-- { true, false, true, false }
```

### Strict Mode (RFC 8259)

```lua
superstring.decode('{"a":1,"a":2}', { strict = true })
-- error: duplicate key "a" not allowed (RFC 8259)

superstring.decode('01', { strict = true })
-- error: leading zeros not allowed (RFC 8259)
```

### Both Together

```lua
local data = superstring.decode(
    '{"enabled":yes,"count":42}',
    { strict = true, superstring = true }
)
```

### Validation

```lua
local ok = superstring.validate('{"a":1}')
-- true

local ok, err = superstring.validate('{"a":1,"a":2}')
-- false, "duplicate key..."

superstring.is_valid_utf8("héllo")   -- true
superstring.is_valid_utf8("\xFF")    -- false
```

### Superstring Token Reference

| Token | Decoded As |
|---|---|
| `true` | `true` |
| `false` | `false` |
| `yes` *(superstring mode)* | `true` |
| `no` *(superstring mode)* | `false` |
| `null` | `superstring.null` |

---

## The Great Error Module

`greaterror.lua` replaceth the silent `nil` errors of common Lua with loud, coloured, informative proclamations. Each error hath a source, a message, optional detail, an optional hint, and a stack trace.

```lua
local greaterror = require("json/greaterror")
```

When a Great Error fires, it printeth to stderr in this manner:

```
╔══ ERROR ══════════════════════════════════════╗
║  source  : json.decode
║  message : unexpected character "}" at position 13
║  detail  : parse failed at byte position 13
║  hint    : check for malformed JSON near position 13
║  trace   :
║    at myfile.lua:10
║    at myfile.lua:5
╚═══════════════════════════════════════════════╝
```

### Raising Errors Directly

```lua
greaterror.error("mymodule", "something went wrong")
greaterror.error("mymodule", "something went wrong", "the value was nil", "check your input")

greaterror.warn("mymodule", "this value is deprecated")
greaterror.info("mymodule", "loaded successfully")
greaterror.fatal("mymodule", "cannot continue")
```

`fatal` printeth the error and calleth `os.exit(1)`.
`error` printeth and then raiseth a Lua error.
`warn` and `info` print only.

### Using `greaterror.try`

Wrappeth a function call and catcheth any error it raiseth, printing it as a Great Error:

```lua
greaterror.try(function()
    json.decode("bad {json")
end, "json.decode")
```

### Using `greaterror.wrap`

Returneth a new function that catcheth errors automatically every time it is called:

```lua
local safe_decode = greaterror.wrap(json.decode, "json.decode")

local result = safe_decode('{"a":1}')
local result2 = safe_decode("bad {json")
```

### Using `greaterror.from_json`

The most powerful pairing — wrappeth any json or superstring call and produceth a Great Error with context-aware hints:

```lua
local greaterror = require("json/greaterror")
local json       = require("json/json")

local result = greaterror.from_json(function()
    return json.decode('{"a":1,"a":2}')
end, "json.decode")

local result2 = greaterror.from_json(function()
    return json.decode_file("missing.json")
end, "json.decode_file")
```

Hints are chosen automatically based on the kind of fault:

| Fault | Hint Given |
|---|---|
| Malformed JSON | Position of the offending character |
| Circular reference | Remove circular references before encoding |
| Max depth exceeded | Reduce nesting or increase MAX\_DEPTH |
| Duplicate key | Remove duplicates or disable strict mode |
| Leading zeros | RFC 8259 forbids leading zeros |
| Bad UTF-8 | Ensure strings are valid UTF-8 |
| PathFiles missing | Ensure PathFiles.lua is beside json.lua |

### Configuring the Great Error

```lua
greaterror.set_level(greaterror.WARNING)
```

Only warnings and above will be displayed. Below the level, errors are silently ignored.

| Level | Code | Behaviour |
|---|---|---|
| `greaterror.FATAL` | 1 | Print + `os.exit(1)` |
| `greaterror.ERROR` | 2 | Print + raise Lua error |
| `greaterror.WARNING` | 3 | Print only |
| `greaterror.INFO` | 4 | Print only |

```lua
greaterror.set_trace(false)
greaterror.set_color(false)
greaterror.reset()
```

`set_trace(false)` — disableth the stack trace in output.
`set_color(false)` — disableth ANSI colour codes (useful for plain log files).
`reset()` — restoreth all settings to their defaults.

### Custom Handler

Thou mayest install thine own handler to receive errors however thou seest fit:

```lua
greaterror.set_handler(function(level, source, message, detail, hint, formatted)
    my_log_system.write(level.label, source, message)
end)
```

When a handler is installed, the default stderr output is suppressed entirely.

---

## Error Handling Reference

All functions throughout this library raise Lua errors on failure. The recommended pattern is `pcall` for fine-grained control, or `greaterror.from_json` for rich output:

```lua
local ok, result = pcall(json.decode, '{"bad":}')
if not ok then
    print("caught:", result)
end

local result = greaterror.from_json(function()
    return json.decode('{"bad":}')
end, "my script")
```

---

## Behaviour Reference

| Situation | Behaviour |
|---|---|
| `nan` / `inf` in encode | Encoded as `null` (error in strict mode) |
| Circular reference | Error thrown |
| Depth over 512 | Error thrown |
| `nil` value in table | Key skipped silently |
| Mixed table (int + string keys) | Encoded as object |
| Sequential integer keys | Encoded as array |
| UTF-8 strings | Passed through as-is |
| `\uXXXX` escape in decode | Converted to UTF-8 |
| Surrogate pairs `\uD800\uDC00` | Correctly decoded to UTF-8 |
| JSON `null` decoded | Returns `json.null` sentinel |
| Trailing garbage | Error thrown |
| Duplicate keys in strict mode | Error thrown |
| Leading zeros in strict mode | Error thrown |
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
