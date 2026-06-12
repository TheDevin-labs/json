local PathFiles = {}

local function trim(s)
	return s:match("^%s*(.-)%s*$")
end

function PathFiles.exists(path)
	local f = io.open(path, "r")
	if f then
		f:close()
		return true
	end
	return false
end

function PathFiles.read(path)
	local f, err = io.open(path, "r")
	if not f then
		error("PathFiles.read: cannot open '" .. path .. "': " .. err)
	end
	local content = f:read("*a")
	f:close()
	return content
end

function PathFiles.read_lines(path)
	local f, err = io.open(path, "r")
	if not f then
		error("PathFiles.read_lines: cannot open '" .. path .. "': " .. err)
	end
	local lines = {}
	for line in f:lines() do
		lines[#lines + 1] = line
	end
	f:close()
	return lines
end

function PathFiles.write(path, content)
	local f, err = io.open(path, "w")
	if not f then
		error("PathFiles.write: cannot open '" .. path .. "': " .. err)
	end
	f:write(content)
	f:close()
end

function PathFiles.append(path, content)
	local f, err = io.open(path, "a")
	if not f then
		error("PathFiles.append: cannot open '" .. path .. "': " .. err)
	end
	f:write(content)
	f:close()
end

function PathFiles.delete(path)
	local ok, err = os.remove(path)
	if not ok then
		error("PathFiles.delete: cannot delete '" .. path .. "': " .. err)
	end
end

function PathFiles.rename(old, new)
	local ok, err = os.rename(old, new)
	if not ok then
		error("PathFiles.rename: cannot rename '" .. old .. "' to '" .. new .. "': " .. err)
	end
end

function PathFiles.copy(src, dst)
	local content = PathFiles.read(src)
	PathFiles.write(dst, content)
end

function PathFiles.mkdir(path)
	local sep = package.config:sub(1, 1)
	local current = ""
	if path:sub(1, 1) == "/" then
		current = "/"
	end
	for part in path:gmatch("[^/\\]+") do
		if current == "" or current == "/" then
			current = current .. part
		else
			current = current .. sep .. part
		end
		if not PathFiles.exists(current) then
			local ok = os.execute("mkdir " .. (sep == "\\" and "" or "-p ") .. '"' .. current .. '"')
			if not ok then
				error("PathFiles.mkdir: cannot create directory '" .. current .. "'")
			end
		end
	end
end

function PathFiles.list(path)
	local sep = package.config:sub(1, 1)
	local cmd
	if sep == "\\" then
		cmd = 'dir /b "' .. path .. '" 2>nul'
	else
		cmd = 'ls -1 "' .. path .. '" 2>/dev/null'
	end
	local handle = io.popen(cmd)
	if not handle then
		error("PathFiles.list: cannot list directory '" .. path .. "'")
	end
	local entries = {}
	for line in handle:lines() do
		line = trim(line)
		if line ~= "" then
			entries[#entries + 1] = line
		end
	end
	handle:close()
	return entries
end

function PathFiles.is_dir(path)
	local sep = package.config:sub(1, 1)
	local cmd
	if sep == "\\" then
		cmd = 'if exist "' .. path .. '\\" (echo yes) else (echo no)'
	else
		cmd = '[ -d "' .. path .. '" ] && echo yes || echo no'
	end
	local handle = io.popen(cmd)
	if not handle then
		return false
	end
	local result = trim(handle:read("*a"))
	handle:close()
	return result == "yes"
end

function PathFiles.is_file(path)
	local sep = package.config:sub(1, 1)
	local cmd
	if sep == "\\" then
		cmd = 'if exist "' .. path .. '" (echo yes) else (echo no)'
	else
		cmd = '[ -f "' .. path .. '" ] && echo yes || echo no'
	end
	local handle = io.popen(cmd)
	if not handle then
		return false
	end
	local result = trim(handle:read("*a"))
	handle:close()
	return result == "yes"
end

function PathFiles.size(path)
	local f, err = io.open(path, "rb")
	if not f then
		error("PathFiles.size: cannot open '" .. path .. "': " .. err)
	end
	local size = f:seek("end")
	f:close()
	return size
end

function PathFiles.join(...)
	local sep = package.config:sub(1, 1)
	local parts = {...}
	local result = {}
	for _, p in ipairs(parts) do
		if p ~= "" then
			result[#result + 1] = p
		end
	end
	return table.concat(result, sep)
end

function PathFiles.basename(path)
	return path:match("[^/\\]+$") or path
end

function PathFiles.dirname(path)
	local dir = path:match("^(.*)[/\\][^/\\]+$")
	return dir or "."
end

function PathFiles.extension(path)
	local ext = path:match("%.([^./\\]+)$")
	return ext or ""
end

function PathFiles.stem(path)
	local base = PathFiles.basename(path)
	return base:match("^(.-)%.[^.]*$") or base
end

return PathFiles
