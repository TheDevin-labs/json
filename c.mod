module cluajit

count as = [library]

lang = C99
abi  = lua-c-api

include cluajit.h
include json.c
include superstring.c
include PathFiles.c
include Makefile

targets = [linux, android, macos, windows]
output  = [json.so, superstring.so, pathfiles.so]
output  = [json.dylib, superstring.dylib, pathfiles.dylib]
output  = [json.dll, superstring.dll, pathfiles.dll]

depends on = json
