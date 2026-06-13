local greaterror = {}

local LEVELS = {
	FATAL   = { code = 1, label = "FATAL"   },
	ERROR   = { code = 2, label = "ERROR"   },
	WARNING = { code = 3, label = "WARNING" },
	INFO    = { code = 4, label = "INFO"    },
}

greaterror.FATAL   = LEVELS.FATAL
greaterror.ERROR   = LEVELS.ERROR
greaterror.WARNING = LEVELS.WARNING
greaterror.INFO    = LEVELS.INFO

local _handler = nil
local _level   = LEVELS.ERROR
local _trace   = true
local _color   = true

local ANSI = {
	reset   = "\27[0m",
	bold    = "\27[1m",
	red     = "\27[31m",
	yellow  = "\27[33m",
	cyan    = "\27[36m",
	white   = "\27[37m",
	magenta = "\27[35m",
}

local function colorize(text, ...)
	if not _color then return text end
	local codes = ""
	for _, c in ipairs({...}) do codes = codes .. c end
	return codes .. text .. ANSI.reset
end

local function level_color(level)
	if level.code == LEVELS.FATAL.code   then return ANSI.red,     ANSI.bold end
	if level.code == LEVELS.ERROR.code   then return ANSI.red,     ""        end
	if level.code == LEVELS.WARNING.code then return ANSI.yellow,  ""        end
	if level.code == LEVELS.INFO.code    then return ANSI.cyan,    ""        end
	return ANSI.white, ""
end

local function build_trace()
	local lines = {}
	local depth = 3
	while true do
		local info = debug and debug.getinfo(depth, "Sl")
		if not info then break end
		local src  = info.short_src or "?"
		local line = info.currentline or 0
		lines[#lines + 1] = "    at " .. src .. ":" .. line
		depth = depth + 1
		if depth > 20 then break end
	end
	return lines
end

local function format_error(level, source, message, detail, hint)
	local c1, c2 = level_color(level)
	local parts = {}

	parts[#parts + 1] = colorize(
		"\n╔══ " .. level.label .. " ══════════════════════════════════════╗",
		c1, c2
	)

	parts[#parts + 1] = colorize(
		"║  source  : " .. (source or "unknown"),
		c1
	)

	parts[#parts + 1] = colorize(
		"║  message : " .. (message or "(no message)"),
		ANSI.white, ANSI.bold
	)

	if detail then
		parts[#parts + 1] = colorize(
			"║  detail  : " .. detail,
			ANSI.white
		)
	end

	if hint then
		parts[#parts + 1] = colorize(
			"║  hint    : " .. hint,
			ANSI.cyan
		)
	end

	if _trace then
		local frames = build_trace()
		if #frames > 0 then
			parts[#parts + 1] = colorize("║  trace   :", ANSI.magenta)
			for _, f in ipairs(frames) do
				parts[#parts + 1] = colorize("║" .. f, ANSI.magenta)
			end
		end
	end

	parts[#parts + 1] = colorize(
		"╚═══════════════════════════════════════════════╝\n",
		c1, c2
	)

	return table.concat(parts, "\n")
end

local function should_handle(level)
	return level.code <= _level.code
end

local function dispatch(level, source, message, detail, hint)
	if not should_handle(level) then return end

	local formatted = format_error(level, source, message, detail, hint)

	if _handler then
		_handler(level, source, message, detail, hint, formatted)
		return
	end

	io.stderr:write(formatted)

	if level.code == LEVELS.FATAL.code then
		os.exit(1)
	end

	if level.code <= LEVELS.ERROR.code then
		error(message, 2)
	end
end

function greaterror.raise(level, source, message, detail, hint)
	dispatch(level, source, message, detail, hint)
end

function greaterror.fatal(source, message, detail, hint)
	dispatch(LEVELS.FATAL, source, message, detail, hint)
end

function greaterror.error(source, message, detail, hint)
	dispatch(LEVELS.ERROR, source, message, detail, hint)
end

function greaterror.warn(source, message, detail, hint)
	dispatch(LEVELS.WARNING, source, message, detail, hint)
end

function greaterror.info(source, message, detail, hint)
	dispatch(LEVELS.INFO, source, message, detail, hint)
end

function greaterror.try(fn, source)
	local ok, err = pcall(fn)
	if not ok then
		dispatch(LEVELS.ERROR, source or "try", err, nil, "wrap your call with greaterror.try to catch errors like this")
	end
	return ok, err
end

function greaterror.wrap(fn, source)
	return function(...)
		local args = {...}
		local ok, err = pcall(function() return fn(table.unpack(args)) end)
		if not ok then
			dispatch(LEVELS.ERROR, source or "wrapped function", err, nil, nil)
			return nil, err
		end
		return err
	end
end

function greaterror.set_handler(fn)
	_handler = fn
end

function greaterror.set_level(level)
	if type(level) ~= "table" or not level.code then
		error("greaterror.set_level: expected a level constant (greaterror.ERROR, etc.)")
	end
	_level = level
end

function greaterror.set_trace(enabled)
	_trace = enabled == true
end

function greaterror.set_color(enabled)
	_color = enabled == true
end

function greaterror.reset()
	_handler = nil
	_level   = LEVELS.ERROR
	_trace   = true
	_color   = true
end

function greaterror.from_json(fn, source)
	local ok, result = pcall(fn)
	if not ok then
		local msg    = tostring(result)
		local detail = nil
		local hint   = nil

		local pos = msg:match("at position (%d+)")
		if pos then
			detail = "parse failed at byte position " .. pos
			hint   = "check for malformed JSON near position " .. pos
		elseif msg:find("circular") then
			hint = "remove circular references from your table before encoding"
		elseif msg:find("max depth") then
			hint = "reduce nesting depth or increase MAX_DEPTH"
		elseif msg:find("duplicate key") then
			hint = "remove duplicate keys or disable strict mode"
		elseif msg:find("leading zeros") then
			hint = "RFC 8259 forbids leading zeros in numbers e.g. use 1 not 01"
		elseif msg:find("UTF%-8") or msg:find("utf") then
			hint = "ensure all strings are valid UTF-8 before encoding"
		elseif msg:find("PathFiles") then
			hint = "ensure PathFiles.lua is in the same directory as json.lua"
		end

		dispatch(LEVELS.ERROR, source or "json", msg, detail, hint)
		return nil, msg
	end
	return result
end

return greaterror
