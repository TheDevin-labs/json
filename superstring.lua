local json = require("json")

local superstring = {}

superstring.null = json.null

function superstring.encode(val, opts)
	opts = opts or {}
	opts.superstring = true
	return json.encode(val, opts)
end

function superstring.decode(s, opts)
	opts = opts or {}
	opts.superstring = true
	return json.decode(s, opts)
end

function superstring.validate(s)
	return json.validate(s, { strict = true, superstring = true })
end

function superstring.is_valid_utf8(s)
	return json.is_valid_utf8(s)
end

return superstring
