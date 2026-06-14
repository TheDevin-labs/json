local json = require("json")

local greaterror = {}

greaterror.FATAL   = json.FATAL
greaterror.ERROR   = json.ERROR
greaterror.WARNING = json.WARNING
greaterror.INFO    = json.INFO

function greaterror.set_handler(fn) json.error_set_handler(fn) end
function greaterror.set_level(l)   json.error_set_level(l)    end
function greaterror.set_trace(b)   json.error_set_trace(b)    end
function greaterror.set_color(b)   json.error_set_color(b)    end
function greaterror.reset()        json.error_reset()         end

function greaterror.try(fn, source)
	local ok, err = pcall(fn)
	if not ok then
		json.error_set_handler(nil)
		local msg = tostring(err)
		io.stderr:write("\n[greaterror.try] " .. (source or "?") .. ": " .. msg .. "\n")
	end
	return ok, err
end

function greaterror.wrap(fn, source)
	return function(...)
		local args = {...}
		local ok, result = pcall(function() return fn(table.unpack(args)) end)
		if not ok then
			io.stderr:write("\n[greaterror.wrap] " .. (source or "?") .. ": " .. tostring(result) .. "\n")
			return nil, result
		end
		return result
	end
end

return greaterror
