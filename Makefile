CC     ?= gcc
CFLAGS ?= -O2 -std=c99 -Wall -Wextra -fPIC

UNAME := $(shell uname -s 2>/dev/null || echo Windows)
ARCH  := $(shell uname -m 2>/dev/null || echo unknown)

ifeq ($(UNAME),Linux)
    ifeq ($(ARCH),aarch64)
        PLATFORM = android
    else
        PLATFORM = linux
    endif
else ifeq ($(UNAME),Darwin)
    PLATFORM = macos
else
    PLATFORM = windows
endif

ifeq ($(PLATFORM),linux)
    EXT     = .so
    LDFLAGS = -shared -fPIC
    LUA_INC ?= $(shell pkg-config --cflags lua 2>/dev/null || echo -I/usr/include/lua5.4)
    LUA_LIB ?= $(shell pkg-config --libs   lua 2>/dev/null || echo -llua5.4)
endif
ifeq ($(PLATFORM),android)
    EXT     = .so
    LDFLAGS = -shared -fPIC
    LUA_INC ?= -I$(PREFIX)/include
    LUA_LIB ?= -llua
endif
ifeq ($(PLATFORM),macos)
    EXT     = .dylib
    LDFLAGS = -dynamiclib -undefined dynamic_lookup
    LUA_INC ?= $(shell pkg-config --cflags lua 2>/dev/null || echo -I/usr/local/include/lua5.4)
    LUA_LIB ?=
endif
ifeq ($(PLATFORM),windows)
    EXT     = .dll
    LDFLAGS = -shared
    LUA_INC ?= -IC:/lua/include
    LUA_LIB ?= -LC:/lua -llua54
endif

.PHONY: all linux android macos windows clean info

all: json$(EXT) superstring$(EXT) pathfiles$(EXT)

linux:   ; $(MAKE) PLATFORM=linux
android: ; $(MAKE) PLATFORM=android
macos:   ; $(MAKE) PLATFORM=macos
windows: ; $(MAKE) PLATFORM=windows CC=x86_64-w64-mingw32-gcc

json$(EXT): json.c cluajit.h
	$(CC) $(CFLAGS) $(LUA_INC) $(LDFLAGS) -o $@ $< $(LUA_LIB) -lm

superstring$(EXT): superstring.c cluajit.h
	$(CC) $(CFLAGS) $(LUA_INC) $(LDFLAGS) -o $@ $< $(LUA_LIB) -lm

pathfiles$(EXT): PathFiles.c cluajit.h
	$(CC) $(CFLAGS) $(LUA_INC) $(LDFLAGS) -o $@ $< $(LUA_LIB)

clean:
	rm -f *.so *.dylib *.dll *.o

info:
	@echo "Platform : $(PLATFORM)"
	@echo "Arch     : $(ARCH)"
	@echo "CC       : $(CC)"
	@echo "LUA_INC  : $(LUA_INC)"
	@echo "LUA_LIB  : $(LUA_LIB)"
