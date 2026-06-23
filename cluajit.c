#ifndef CLUAJIT_H
#define CLUAJIT_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "lua.h"
#include "lauxlib.h"
#include "lualib.h"

#if LUA_VERSION_NUM >= 502
#define cj_rawlen(L,i) lua_rawlen(L,i)
#else
#define cj_rawlen(L,i) lua_objlen(L,i)
#endif

#if LUA_VERSION_NUM >= 503
#define cj_isinteger(L,i) lua_isinteger(L,i)
#else
#define cj_isinteger(L,i) 0
#endif

#define CLUAJIT_MAX_DEPTH 512
#define CLUAJIT_BUF_INIT  4096

typedef struct {
    char   *data;
    size_t  len;
    size_t  cap;
} Buffer;

typedef struct {
    const char *s;
    size_t      len;
    size_t      pos;
    int         superstring;
    int         strict;
} Parser;

int  buf_init(Buffer *b);
void buf_free(Buffer *b);
int  buf_grow(Buffer *b, size_t need);
int  buf_push(Buffer *b, const char *s, size_t n);
int  buf_pushc(Buffer *b, char c);
int  buf_pushs(Buffer *b, const char *s);

int luaopen_json(lua_State *L);
int luaopen_superstring(lua_State *L);
int luaopen_pathfiles(lua_State *L);

#endif
