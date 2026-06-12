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

local function encode_string(s)
	s = s:gsub('[\\"/%c]', function(c)
		return ESCAPE_MAP[c] or string.format('\\u%04x', c:byte())
	end)
	return '"' .. s .. '"'
end

local function is_array(t)
	local max = 0
	local count = 0
	for k, _ in pairs(t) do
		if type(k) ~= 'number' or k ~= math.floor(k) or k < 1 then
			return false
		end
		if k > max then max = k end
		count = count + 1
	end
	return max == count
end

local function encode_value(val, opts, depth, seen)
	if depth > MAX_DEPTH then
		error('json.encode: max depth of ' .. MAX_DEPTH .. ' exceeded')
	end

	local t = type(val)

	if val == json.null then
		return 'null'
	elseif t == 'nil' then
		return 'null'
	elseif t == 'boolean' then
		return tostring(val)
	elseif t == 'number' then
		if val ~= val or val == math.huge or val == -math.huge then
			return 'null'
		end
		if val == math.floor(val) and math.abs(val) < 1e15 then
			return string.format('%d', val)
		end
		return string.format('%.17g', val)
	elseif t == 'string' then
		return encode_string(val)
	elseif t == 'table' then
		if seen[val] then
			error('json.encode: circular reference detected')
		end
		seen[val] = true

		local result
		local indent = opts.indent or 2
		local pretty = opts.pretty or false
		local sort_keys = opts.sort_keys or false
		local sp = string.rep(' ', indent)
		local pad = string.rep(' ', indent * (depth))
		local pad_inner = pad .. sp

		if is_array(val) then
			local items = {}
			for i = 1, #val do
				local v = val[i]
				if v == nil then
					items[#items + 1] = 'null'
				else
					items[#items + 1] = encode_value(v, opts, depth + 1, seen)
				end
			end
			if pretty then
				if #items == 0 then
					result = '[]'
				else
					result = '[\n' .. pad_inner .. table.concat(items, ',\n' .. pad_inner) .. '\n' .. pad .. ']'
				end
			else
				result = '[' .. table.concat(items, ',') .. ']'
			end
		else
			local keys = {}
			for k, v in pairs(val) do
				if type(k) == 'string' or type(k) == 'number' then
					if v ~= nil then
						keys[#keys + 1] = k
					end
				end
			end
			if sort_keys then
				table.sort(keys, function(a, b)
					return tostring(a) < tostring(b)
				end)
			end
			local items = {}
			for _, k in ipairs(keys) do
				local v = val[k]
				if v ~= nil then
					local key_str = encode_string(tostring(k))
					local val_str = encode_value(v, opts, depth + 1, seen)
					if pretty then
						items[#items + 1] = key_str .. ': ' .. val_str
					else
						items[#items + 1] = key_str .. ':' .. val_str
					end
				end
			end
			if pretty then
				if #items == 0 then
					result = '{}'
				else
					result = '{\n' .. pad_inner .. table.concat(items, ',\n' .. pad_inner) .. '\n' .. pad .. '}'
				end
			else
				result = '{' .. table.concat(items, ',') .. '}'
			end
		end

		seen[val] = nil
		return result
	else
		error('json.encode: unsupported type: ' .. t)
	end
end

function json.encode(val, opts)
	opts = opts or {}
	return encode_value(val, opts, 0, {})
end

local function skip_whitespace(s, i)
	while i <= #s do
		local c = s:sub(i, i)
		if c == ' ' or c == '\t' or c == '\n' or c == '\r' then
			i = i + 1
		else
			break
		end
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
			if e == '"' then
				parts[#parts + 1] = '"'
				i = i + 2
			elseif e == '\\' then
				parts[#parts + 1] = '\\'
				i = i + 2
			elseif e == '/' then
				parts[#parts + 1] = '/'
				i = i + 2
			elseif e == 'b' then
				parts[#parts + 1] = '\b'
				i = i + 2
			elseif e == 'f' then
				parts[#parts + 1] = '\f'
				i = i + 2
			elseif e == 'n' then
				parts[#parts + 1] = '\n'
				i = i + 2
			elseif e == 'r' then
				parts[#parts + 1] = '\r'
				i = i + 2
			elseif e == 't' then
				parts[#parts + 1] = '\t'
				i = i + 2
			elseif e == 'u' then
				local hex = s:sub(i + 2, i + 5)
				if #hex < 4 then
					error('json.decode: invalid \\u escape at position ' .. i)
				end
				local codepoint = tonumber(hex, 16)
				if not codepoint then
					error('json.decode: invalid \\u escape at position ' .. i)
				end
				if codepoint >= 0xD800 and codepoint <= 0xDBFF then
					if s:sub(i + 6, i + 7) ~= '\\u' then
						error('json.decode: missing low surrogate at position ' .. i)
					end
					local hex2 = s:sub(i + 8, i + 11)
					local low = tonumber(hex2, 16)
					if not low or low < 0xDC00 or low > 0xDFFF then
						error('json.decode: invalid low surrogate at position ' .. i)
					end
					codepoint = 0x10000 + (codepoint - 0xD800) * 0x400 + (low - 0xDC00)
					i = i + 12
				else
					i = i + 6
				end
				if codepoint < 0x80 then
					parts[#parts + 1] = string.char(codepoint)
				elseif codepoint < 0x800 then
					parts[#parts + 1] = string.char(
						0xC0 + math.floor(codepoint / 64),
						0x80 + (codepoint % 64)
					)
				elseif codepoint < 0x10000 then
					parts[#parts + 1] = string.char(
						0xE0 + math.floor(codepoint / 4096),
						0x80 + math.floor(codepoint / 64) % 64,
						0x80 + (codepoint % 64)
					)
				else
					parts[#parts + 1] = string.char(
						0xF0 + math.floor(codepoint / 262144),
						0x80 + math.floor(codepoint / 4096) % 64,
						0x80 + math.floor(codepoint / 64) % 64,
						0x80 + (codepoint % 64)
					)
				end
			else
				error('json.decode: invalid escape \\' .. e .. ' at position ' .. i)
			end
		else
			parts[#parts + 1] = c
			i = i + 1
		end
	end
	error('json.decode: unterminated string')
end

local function decode_number(s, i)
	local j = i
	if s:sub(j, j) == '-' then j = j + 1 end
	while j <= #s and s:sub(j, j):match('%d') do j = j + 1 end
	if s:sub(j, j) == '.' then
		j = j + 1
		while j <= #s and s:sub(j, j):match('%d') do j = j + 1 end
	end
	if s:sub(j, j):match('[eE]') then
		j = j + 1
		if s:sub(j, j):match('[+-]') then j = j + 1 end
		while j <= #s and s:sub(j, j):match('%d') do j = j + 1 end
	end
	local num_str = s:sub(i, j - 1)
	local num = tonumber(num_str)
	if not num then
		error('json.decode: invalid number at position ' .. i)
	end
	return num, j
end

local function decode_array(s, i, depth)
	if depth > MAX_DEPTH then
		error('json.decode: max depth of ' .. MAX_DEPTH .. ' exceeded')
	end
	i = i + 1
	local arr = {}
	i = skip_whitespace(s, i)
	if s:sub(i, i) == ']' then
		return arr, i + 1
	end
	while true do
		i = skip_whitespace(s, i)
		local val
		val, i = decode_value(s, i, depth + 1)
		arr[#arr + 1] = val
		i = skip_whitespace(s, i)
		local c = s:sub(i, i)
		if c == ']' then
			return arr, i + 1
		elseif c == ',' then
			i = i + 1
		else
			error('json.decode: expected , or ] at position ' .. i)
		end
	end
end

local function decode_object(s, i, depth)
	if depth > MAX_DEPTH then
		error('json.decode: max depth of ' .. MAX_DEPTH .. ' exceeded')
	end
	i = i + 1
	local obj = {}
	i = skip_whitespace(s, i)
	if s:sub(i, i) == '}' then
		return obj, i + 1
	end
	while true do
		i = skip_whitespace(s, i)
		if s:sub(i, i) ~= '"' then
			error('json.decode: expected string key at position ' .. i)
		end
		local key
		key, i = decode_string(s, i)
		i = skip_whitespace(s, i)
		if s:sub(i, i) ~= ':' then
			error('json.decode: expected : at position ' .. i)
		end
		i = i + 1
		i = skip_whitespace(s, i)
		local val
		val, i = decode_value(s, i, depth + 1)
		obj[key] = val
		i = skip_whitespace(s, i)
		local c = s:sub(i, i)
		if c == '}' then
			return obj, i + 1
		elseif c == ',' then
			i = i + 1
		else
			error('json.decode: expected , or } at position ' .. i)
		end
	end
end

decode_value = function(s, i, depth)
	depth = depth or 0
	i = skip_whitespace(s, i)
	if i > #s then
		error('json.decode: unexpected end of input')
	end
	local c = s:sub(i, i)
	if c == '"' then
		return decode_string(s, i)
	elseif c == '{' then
		return decode_object(s, i, depth)
	elseif c == '[' then
		return decode_array(s, i, depth)
	elseif c == 't' then
		if s:sub(i, i + 3) == 'true' then
			return true, i + 4
		end
		error('json.decode: invalid token at position ' .. i)
	elseif c == 'f' then
		if s:sub(i, i + 4) == 'false' then
			return false, i + 5
		end
		error('json.decode: invalid token at position ' .. i)
	elseif c == 'n' then
		if s:sub(i, i + 3) == 'null' then
			return json.null, i + 4
		end
		error('json.decode: invalid token at position ' .. i)
	elseif c == '-' or c:match('%d') then
		return decode_number(s, i)
	else
		error('json.decode: unexpected character "' .. c .. '" at position ' .. i)
	end
end

function json.decode(s)
	if type(s) ~= 'string' then
		error('json.decode: expected string, got ' .. type(s))
	end
	local val, i = decode_value(s, 1, 0)
	i = skip_whitespace(s, i)
	if i <= #s then
		error('json.decode: trailing garbage at position ' .. i)
	end
	return val
end

function json.encode_file(path, val, opts)
	if not has_pathfiles then
		error('json.encode_file: PathFiles is not available')
	end
	local content = json.encode(val, opts)
	PathFiles.write(path, content)
end

function json.decode_file(path)
	if not has_pathfiles then
		error('json.decode_file: PathFiles is not available')
	end
	local content = PathFiles.read(path)
	return json.decode(content)
end

function json.append_file(path, val, opts)
	if not has_pathfiles then
		error('json.append_file: PathFiles is not available')
	end
	local content = json.encode(val, opts) .. '\n'
	PathFiles.append(path, content)
end

function json.decode_lines(path)
	if not has_pathfiles then
		error('json.decode_lines: PathFiles is not available')
	end
	local lines = PathFiles.read_lines(path)
	local results = {}
	for _, line in ipairs(lines) do
		line = line:match("^%s*(.-)%s*$")
		if line ~= '' then
			results[#results + 1] = json.decode(line)
		end
	end
	return results
end

function json.exists(path)
	if not has_pathfiles then
		error('json.exists: PathFiles is not available')
	end
	return PathFiles.exists(path)
end

function json.has_pathfiles()
	return has_pathfiles
end

return json
