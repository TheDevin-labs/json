local json = {}

local _pf_ok, PathFiles = pcall(require, "PathFiles")
local has_pathfiles = _pf_ok and type(PathFiles) == "table"

json.null = setmetatable({}, {
	__tostring = function() return "null" end,
	__newindex = function() error("json.null is immutable") end,
})

local MAX_DEPTH = 512

local ESCAPE_MAP = {
	['"']  = '\\"',
	['\\'] = '\\\\',
	['\b'] = '\\b',
	['\f'] = '\\f',
	['\n'] = '\\n',
	['\r'] = '\\r',
	['\t'] = '\\t',
}

local _ge_handler = nil
local _ge_level   = 2
local _ge_trace   = true
local _ge_color   = true

local ANSI = {
	reset   = "\27[0m",
	bold    = "\27[1m",
	red     = "\27[31m",
	yellow  = "\27[33m",
	cyan    = "\27[36m",
	white   = "\27[37m",
	magenta = "\27[35m",
}

local LEVEL_FATAL   = { code = 1, label = "FATAL"   }
local LEVEL_ERROR   = { code = 2, label = "ERROR"   }
local LEVEL_WARNING = { code = 3, label = "WARNING" }
local LEVEL_INFO    = { code = 4, label = "INFO"    }

json.FATAL   = LEVEL_FATAL
json.ERROR   = LEVEL_ERROR
json.WARNING = LEVEL_WARNING
json.INFO    = LEVEL_INFO

local function colorize(text, ...)
	if not _ge_color then return text end
	local codes = ""
	for _, c in ipairs({...}) do codes = codes .. c end
	return codes .. text .. ANSI.reset
end

local function level_color(level)
	if level.code == 1 then return ANSI.red,    ANSI.bold end
	if level.code == 2 then return ANSI.red,    ""        end
	if level.code == 3 then return ANSI.yellow, ""        end
	if level.code == 4 then return ANSI.cyan,   ""        end
	return ANSI.white, ""
end

local function build_trace()
	if not debug then return {} end
	local lines = {}
	local depth = 4
	while depth <= 24 do
		local info = debug.getinfo(depth, "Sl")
		if not info then break end
		local src  = info.short_src or "?"
		local line = info.currentline or 0
		lines[#lines + 1] = "    at " .. src .. ":" .. line
		depth = depth + 1
	end
	return lines
end

local function format_ge(level, source, message, detail, hint)
	local c1, c2 = level_color(level)
	local parts  = {}
	parts[#parts + 1] = colorize("\n╔══ " .. level.label .. " ══════════════════════════════════════╗", c1, c2)
	parts[#parts + 1] = colorize("║  source  : " .. (source  or "unknown"),       c1)
	parts[#parts + 1] = colorize("║  message : " .. (message or "(no message)"),  ANSI.white, ANSI.bold)
	if detail then
		parts[#parts + 1] = colorize("║  detail  : " .. detail, ANSI.white)
	end
	if hint then
		parts[#parts + 1] = colorize("║  hint    : " .. hint, ANSI.cyan)
	end
	if _ge_trace then
		local frames = build_trace()
		if #frames > 0 then
			parts[#parts + 1] = colorize("║  trace   :", ANSI.magenta)
			for _, f in ipairs(frames) do
				parts[#parts + 1] = colorize("║" .. f, ANSI.magenta)
			end
		end
	end
	parts[#parts + 1] = colorize("╚═══════════════════════════════════════════════╝\n", c1, c2)
	return table.concat(parts, "\n")
end

local function ge_dispatch(level, source, message, detail, hint)
	if level.code > _ge_level then return end
	local formatted = format_ge(level, source, message, detail, hint)
	if _ge_handler then
		_ge_handler(level, source, message, detail, hint, formatted)
		return
	end
	io.stderr:write(formatted)
	if level.code == 1 then os.exit(1) end
	if level.code <= 2 then error(message, 3) end
end

local function _raise(source, msg, detail, hint)
	ge_dispatch(LEVEL_ERROR, source, msg, detail, hint)
end

local function validate_utf8(s)
	local i = 1
	while i <= #s do
		local b = s:byte(i)
		if b < 0x80 then
			i = i + 1
		elseif b >= 0xC2 and b <= 0xDF then
			if i + 1 > #s then return false, i end
			local b2 = s:byte(i + 1)
			if b2 < 0x80 or b2 > 0xBF then return false, i end
			i = i + 2
		elseif b >= 0xE0 and b <= 0xEF then
			if i + 2 > #s then return false, i end
			local b2, b3 = s:byte(i + 1), s:byte(i + 2)
			if b == 0xE0 and (b2 < 0xA0 or b2 > 0xBF) then return false, i end
			if b == 0xED and (b2 < 0x80 or b2 > 0x9F) then return false, i end
			if b2 < 0x80 or b2 > 0xBF then return false, i end
			if b3 < 0x80 or b3 > 0xBF then return false, i end
			i = i + 3
		elseif b >= 0xF0 and b <= 0xF4 then
			if i + 3 > #s then return false, i end
			local b2, b3, b4 = s:byte(i+1), s:byte(i+2), s:byte(i+3)
			if b == 0xF0 and (b2 < 0x90 or b2 > 0xBF) then return false, i end
			if b == 0xF4 and (b2 < 0x80 or b2 > 0x8F) then return false, i end
			if b2 < 0x80 or b2 > 0xBF then return false, i end
			if b3 < 0x80 or b3 > 0xBF then return false, i end
			if b4 < 0x80 or b4 > 0xBF then return false, i end
			i = i + 4
		else
			return false, i
		end
	end
	return true
end

local function encode_string(s, strict)
	if strict then
		local ok, pos = validate_utf8(s)
		if not ok then
			_raise('json.encode', 'invalid UTF-8 byte at position ' .. pos, 'in string: ' .. s:sub(1, 40), 'ensure all strings are valid UTF-8 before encoding')
		end
	end
	s = s:gsub('[\\"/%c]', function(c)
		return ESCAPE_MAP[c] or string.format('\\u%04x', c:byte())
	end)
	return '"' .. s .. '"'
end

local function is_array(t)
	local max, count = 0, 0
	for k, _ in pairs(t) do
		if type(k) ~= 'number' or k ~= math.floor(k) or k < 1 then return false end
		if k > max then max = k end
		count = count + 1
	end
	return max == count
end

local function encode_value(val, opts, depth, seen)
	if depth > MAX_DEPTH then
		_raise('json.encode', 'max depth of ' .. MAX_DEPTH .. ' exceeded', nil, 'reduce nesting depth in your table')
	end

	local strict   = opts.strict      or false
	local yesno    = opts.superstring or false
	local t        = type(val)

	if val == json.null then
		return 'null'
	elseif t == 'nil' then
		return 'null'
	elseif t == 'boolean' then
		if yesno then return val and 'yes' or 'no' end
		return tostring(val)
	elseif t == 'number' then
		if val ~= val or val == math.huge or val == -math.huge then
			if strict then
				_raise('json.encode', 'non-finite number is not allowed in strict mode', tostring(val), 'replace nan/inf with a real number or json.null')
			end
			return 'null'
		end
		if val == math.floor(val) and math.abs(val) < 1e15 then
			return string.format('%d', val)
		end
		return string.format('%.17g', val)
	elseif t == 'string' then
		return encode_string(val, strict)
	elseif t == 'table' then
		if seen[val] then
			_raise('json.encode', 'circular reference detected', nil, 'remove circular references from your table before encoding')
		end
		seen[val] = true

		local result
		local indent    = opts.indent    or 2
		local pretty    = opts.pretty    or false
		local sort_keys = opts.sort_keys or false
		local sp        = string.rep(' ', indent)
		local pad       = string.rep(' ', indent * depth)
		local pad_inner = pad .. sp

		if is_array(val) then
			local items = {}
			for i = 1, #val do
				local v = val[i]
				items[#items + 1] = v == nil and 'null' or encode_value(v, opts, depth + 1, seen)
			end
			if pretty then
				result = #items == 0 and '[]'
					or '[\n' .. pad_inner .. table.concat(items, ',\n' .. pad_inner) .. '\n' .. pad .. ']'
			else
				result = '[' .. table.concat(items, ',') .. ']'
			end
		else
			local keys = {}
			for k, v in pairs(val) do
				if (type(k) == 'string' or type(k) == 'number') and v ~= nil then
					keys[#keys + 1] = k
				end
			end
			if sort_keys then
				table.sort(keys, function(a, b) return tostring(a) < tostring(b) end)
			end
			local items = {}
			for _, k in ipairs(keys) do
				local v = val[k]
				if v ~= nil then
					local ks = encode_string(tostring(k), strict)
					local vs = encode_value(v, opts, depth + 1, seen)
					items[#items + 1] = pretty and (ks .. ': ' .. vs) or (ks .. ':' .. vs)
				end
			end
			if pretty then
				result = #items == 0 and '{}'
					or '{\n' .. pad_inner .. table.concat(items, ',\n' .. pad_inner) .. '\n' .. pad .. '}'
			else
				result = '{' .. table.concat(items, ',') .. '}'
			end
		end

		seen[val] = nil
		return result
	else
		_raise('json.encode', 'unsupported type: ' .. t, nil, 'only table, string, number, boolean, and json.null are supported')
	end
end

function json.encode(val, opts)
	opts = opts or {}
	return encode_value(val, opts, 0, {})
end

local function skip_ws(s, i)
	while i <= #s do
		local c = s:sub(i, i)
		if c ~= ' ' and c ~= '\t' and c ~= '\n' and c ~= '\r' then break end
		i = i + 1
	end
	return i
end

local decode_value

local function decode_string(s, i)
	i = i + 1
	local parts = {}
	while i <= #s do
		local c = s:sub(i, i)
		if c == '"' then
			return table.concat(parts), i + 1
		elseif c == '\\' then
			local e = s:sub(i + 1, i + 1)
			if     e == '"'  then parts[#parts+1] = '"';  i = i + 2
			elseif e == '\\' then parts[#parts+1] = '\\'; i = i + 2
			elseif e == '/'  then parts[#parts+1] = '/';  i = i + 2
			elseif e == 'b'  then parts[#parts+1] = '\b'; i = i + 2
			elseif e == 'f'  then parts[#parts+1] = '\f'; i = i + 2
			elseif e == 'n'  then parts[#parts+1] = '\n'; i = i + 2
			elseif e == 'r'  then parts[#parts+1] = '\r'; i = i + 2
			elseif e == 't'  then parts[#parts+1] = '\t'; i = i + 2
			elseif e == 'u' then
				local hex = s:sub(i + 2, i + 5)
				if #hex < 4 then
					_raise('json.decode', 'invalid \\u escape at position ' .. i, nil, 'unicode escapes must be exactly 4 hex digits e.g. \\u0041')
				end
				local cp = tonumber(hex, 16)
				if not cp then
					_raise('json.decode', 'invalid \\u escape at position ' .. i, 'hex: ' .. hex, 'check that all 4 characters are valid hex 0-9 a-f')
				end
				if cp >= 0xD800 and cp <= 0xDBFF then
					if s:sub(i + 6, i + 7) ~= '\\u' then
						_raise('json.decode', 'missing low surrogate at position ' .. i, nil, 'high surrogate must be followed by \\uDC00-\\uDFFF')
					end
					local hex2 = s:sub(i + 8, i + 11)
					local low  = tonumber(hex2, 16)
					if not low or low < 0xDC00 or low > 0xDFFF then
						_raise('json.decode', 'invalid low surrogate at position ' .. i, 'got: ' .. (hex2 or '?'), 'low surrogate must be in range \\uDC00-\\uDFFF')
					end
					cp = 0x10000 + (cp - 0xD800) * 0x400 + (low - 0xDC00)
					i  = i + 12
				else
					i = i + 6
				end
				if cp < 0x80 then
					parts[#parts+1] = string.char(cp)
				elseif cp < 0x800 then
					parts[#parts+1] = string.char(0xC0 + math.floor(cp/64), 0x80 + (cp%64))
				elseif cp < 0x10000 then
					parts[#parts+1] = string.char(0xE0+math.floor(cp/4096), 0x80+math.floor(cp/64)%64, 0x80+(cp%64))
				else
					parts[#parts+1] = string.char(0xF0+math.floor(cp/262144), 0x80+math.floor(cp/4096)%64, 0x80+math.floor(cp/64)%64, 0x80+(cp%64))
				end
			else
				_raise('json.decode', 'invalid escape \\' .. e .. ' at position ' .. i, nil, 'valid escapes: \\" \\\\ \\/ \\b \\f \\n \\r \\t \\uXXXX')
			end
		else
			parts[#parts+1] = c
			i = i + 1
		end
	end
	_raise('json.decode', 'unterminated string', nil, 'ensure every opening " has a closing "')
end

local function decode_number(s, i, strict)
	local j = i
	if s:sub(j,j) == '-' then j = j + 1 end
	local ns = j
	while j <= #s and s:sub(j,j):match('%d') do j = j + 1 end
	if strict then
		local digits = s:sub(ns, j-1)
		if #digits > 1 and digits:sub(1,1) == '0' then
			_raise('json.decode', 'leading zeros not allowed (RFC 8259) at position ' .. i, nil, 'use ' .. tonumber(digits) .. ' instead of ' .. digits)
		end
	end
	if s:sub(j,j) == '.' then
		j = j + 1
		local fs = j
		while j <= #s and s:sub(j,j):match('%d') do j = j + 1 end
		if strict and j == fs then
			_raise('json.decode', 'digit required after decimal point at position ' .. j, nil, 'e.g. use 1.0 not 1.')
		end
	end
	if s:sub(j,j):match('[eE]') then
		j = j + 1
		if s:sub(j,j):match('[+-]') then j = j + 1 end
		local es = j
		while j <= #s and s:sub(j,j):match('%d') do j = j + 1 end
		if strict and j == es then
			_raise('json.decode', 'digit required in exponent at position ' .. j, nil, 'e.g. use 1e2 not 1e')
		end
	end
	local num = tonumber(s:sub(i, j-1))
	if not num then
		_raise('json.decode', 'invalid number at position ' .. i, 'got: ' .. s:sub(i, j-1), 'check for malformed numeric literals')
	end
	return num, j
end

local function decode_array(s, i, depth, opts)
	if depth > MAX_DEPTH then
		_raise('json.decode', 'max depth of ' .. MAX_DEPTH .. ' exceeded', nil, 'reduce nesting depth')
	end
	i = i + 1
	local arr = {}
	i = skip_ws(s, i)
	if s:sub(i,i) == ']' then return arr, i + 1 end
	while true do
		i = skip_ws(s, i)
		local val
		val, i = decode_value(s, i, depth + 1, opts)
		arr[#arr+1] = val
		i = skip_ws(s, i)
		local c = s:sub(i,i)
		if     c == ']' then return arr, i + 1
		elseif c == ',' then i = i + 1
		else _raise('json.decode', 'expected , or ] at position ' .. i, 'got: "' .. c .. '"', 'array elements must be separated by commas') end
	end
end

local function decode_object(s, i, depth, opts)
	if depth > MAX_DEPTH then
		_raise('json.decode', 'max depth of ' .. MAX_DEPTH .. ' exceeded', nil, 'reduce nesting depth')
	end
	local strict = opts and opts.strict or false
	i = i + 1
	local obj, seen_keys = {}, {}
	i = skip_ws(s, i)
	if s:sub(i,i) == '}' then return obj, i + 1 end
	while true do
		i = skip_ws(s, i)
		if s:sub(i,i) ~= '"' then
			_raise('json.decode', 'expected string key at position ' .. i, 'got: "' .. s:sub(i,i) .. '"', 'object keys must be quoted strings')
		end
		local key
		key, i = decode_string(s, i)
		if strict then
			if seen_keys[key] then
				_raise('json.decode', 'duplicate key "' .. key .. '" not allowed (RFC 8259)', nil, 'remove duplicate keys or disable strict mode')
			end
			seen_keys[key] = true
		end
		i = skip_ws(s, i)
		if s:sub(i,i) ~= ':' then
			_raise('json.decode', 'expected : at position ' .. i, 'got: "' .. s:sub(i,i) .. '"', 'key and value must be separated by a colon')
		end
		i = i + 1
		i = skip_ws(s, i)
		local val
		val, i = decode_value(s, i, depth + 1, opts)
		obj[key] = val
		i = skip_ws(s, i)
		local c = s:sub(i,i)
		if     c == '}' then return obj, i + 1
		elseif c == ',' then i = i + 1
		else _raise('json.decode', 'expected , or } at position ' .. i, 'got: "' .. c .. '"', 'object entries must be separated by commas') end
	end
end

decode_value = function(s, i, depth, opts)
	depth = depth or 0
	opts  = opts  or {}
	local yesno  = opts.superstring or false
	local strict = opts.strict      or false
	i = skip_ws(s, i)
	if i > #s then
		_raise('json.decode', 'unexpected end of input', nil, 'the JSON string may be truncated or empty')
	end
	local c = s:sub(i,i)
	if     c == '"' then return decode_string(s, i)
	elseif c == '{' then return decode_object(s, i, depth, opts)
	elseif c == '[' then return decode_array(s, i, depth, opts)
	elseif c == 't' then
		if s:sub(i, i+3) == 'true'  then return true,  i + 4 end
		_raise('json.decode', 'invalid token at position ' .. i, 'got: "' .. s:sub(i,i+4) .. '"', 'did you mean true?')
	elseif c == 'f' then
		if s:sub(i, i+4) == 'false' then return false, i + 5 end
		_raise('json.decode', 'invalid token at position ' .. i, 'got: "' .. s:sub(i,i+4) .. '"', 'did you mean false?')
	elseif c == 'n' then
		if s:sub(i, i+3) == 'null' then return json.null, i + 4 end
		if yesno and s:sub(i, i+1) == 'no' then return false, i + 2 end
		_raise('json.decode', 'invalid token at position ' .. i, 'got: "' .. s:sub(i,i+3) .. '"', 'did you mean null?')
	elseif c == 'y' then
		if yesno and s:sub(i, i+2) == 'yes' then return true, i + 3 end
		_raise('json.decode', 'invalid token at position ' .. i, 'got: "' .. s:sub(i,i+2) .. '"', 'yes/no tokens require { superstring = true } option')
	elseif c == '-' or c:match('%d') then
		return decode_number(s, i, strict)
	else
		_raise('json.decode', 'unexpected character "' .. c .. '" at position ' .. i, nil, 'check for invalid characters or unquoted strings')
	end
end

function json.decode(s, opts)
	opts = opts or {}
	if type(s) ~= 'string' then
		_raise('json.decode', 'expected string, got ' .. type(s), nil, 'pass a JSON string to json.decode')
	end
	local val, i = decode_value(s, 1, 0, opts)
	i = skip_ws(s, i)
	if i <= #s then
		_raise('json.decode', 'trailing garbage at position ' .. i, 'remaining: "' .. s:sub(i, i+10) .. '"', 'the JSON value ended but the string continues')
	end
	return val
end

function json.validate(s, opts)
	local ok, err = pcall(json.decode, s, opts or { strict = true })
	if ok then return true end
	return false, err
end

function json.is_valid_utf8(s)
	return validate_utf8(s)
end

function json.encode_file(path, val, opts)
	if not has_pathfiles then
		_raise('json.encode_file', 'PathFiles is not available', nil, 'ensure PathFiles.lua is in the same directory as json.lua')
	end
	PathFiles.write(path, json.encode(val, opts))
end

function json.decode_file(path, opts)
	if not has_pathfiles then
		_raise('json.decode_file', 'PathFiles is not available', nil, 'ensure PathFiles.lua is in the same directory as json.lua')
	end
	return json.decode(PathFiles.read(path), opts)
end

function json.append_file(path, val, opts)
	if not has_pathfiles then
		_raise('json.append_file', 'PathFiles is not available', nil, 'ensure PathFiles.lua is in the same directory as json.lua')
	end
	PathFiles.append(path, json.encode(val, opts) .. '\n')
end

function json.decode_lines(path, opts)
	if not has_pathfiles then
		_raise('json.decode_lines', 'PathFiles is not available', nil, 'ensure PathFiles.lua is in the same directory as json.lua')
	end
	local results = {}
	for _, line in ipairs(PathFiles.read_lines(path)) do
		line = line:match("^%s*(.-)%s*$")
		if line ~= '' then results[#results+1] = json.decode(line, opts) end
	end
	return results
end

function json.exists(path)
	if not has_pathfiles then
		_raise('json.exists', 'PathFiles is not available', nil, 'ensure PathFiles.lua is in the same directory as json.lua')
	end
	return PathFiles.exists(path)
end

function json.error_set_handler(fn) _ge_handler = fn end
function json.error_set_level(l)
	if type(l) ~= 'table' or not l.code then
		error('json.error_set_level: expected a level constant e.g. json.ERROR')
	end
	_ge_level = l.code
end
function json.error_set_trace(b) _ge_trace = b == true end
function json.error_set_color(b) _ge_color = b == true end
function json.error_reset()
	_ge_handler = nil
	_ge_level   = 2
	_ge_trace   = true
	_ge_color   = true
end

function json.has_pathfiles() return has_pathfiles end

return json
