# JSON

A most wondrous, lightweight, and local library, fashioned in the native tongue of Lua for the encoding, decoding, and stewardship of JSON metadata and file operations.

[![License](https://img.shields.io/badge/License-BSD__2--Clause-blue.svg)](https://opensource.org/licenses/BSD-2-Clause)
![Language](https://img.shields.io/badge/Language-Lua-blue)

---

> [!NOTE]
> Take heed, gentle practitioner! This library abideth wholly in isolation, utterly severed from the global web. Wherefore, whenever a new craft or amendment is brought forth, thou art required to fetch and install it locally upon thy machine. Be not troubled, for no future revision shall break or confound the work thou hast already established.

## Notable Attributes

* **On File Stewardship:** Containeth a dedicated module (PathFiles.lua) to navigate and govern thy file systems with great ease.
* **Swift & Unburdened:** Wrought in pure Lua, demanding no external alliances nor strange dependencies.
* **Secure & Recluse:** Absent of all internet commerce, thereby ensuring the absolute privacy of thy data.

## How One May Import This Craft

Follow these precise steps to graft this library into thine own Lua endeavor:

1. **Deposit the Files:** Convey the essential files (json.lua, PathFiles.lua) and deposit them safely within a designated folder inside thy project directory (perchance named "json").

2. **Invoke the Code:** Bind the library at the very summit of thy script, employing the solemn syntax written hereunder:
   ```lua
   local json = require("json/json")

```
## Examples of the Craft
### 1. Translating a Lua Table into JSON Text (Encoding)
Shouldst thou desire to convert thy native Lua structures into a clean JSON parchment, employ this method:
```lua
local json = require("json/json")

local character_profile = {
    username = "CoolyDucks",
    guild = "TheDevinLabs",
    is_active = true,
    projects_count = 3,
    languages = { "Lua", "JSON", "Markdown" }
}

-- Turn the structure into a string of text
local json_parchment = json.encode(character_profile)
print(json_parchment)

```
### 2. Translating JSON Text Back into Lua (Decoding)
When receiving raw JSON text from a ledger or file, thou canst easily restore it back into a proper Lua table:
```lua
local json = require("json/json")

local raw_scroll = '{"guild": "TheDevinLabs", "status": "Active"}'

-- Decode the raw text into a usable table
local data_table = json.decode(raw_scroll)
print("The Guild Name is: " .. data_table.guild)

```
## License & Covenant of Fair Use
This work is bound by the terms of the BSD 2-Clause Simplified License.
 * **An Open Source:** Thou art granted full liberty to employ, alter, and distribute these codes as seest fit.
 * **A Solemn Disclaimer:** Whilst thou mayest use this craft for any noble purpose, thou shalt not covet nor steal the name of this project, nor falsely proclaim it as thine own invention.
Maintained with due diligence by the guild of TheDevinLabs
