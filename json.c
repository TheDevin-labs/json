#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <errno.h>
#include <sys/stat.h>

#ifdef _WIN32
#include <windows.h>
#include <direct.h>
#define PATH_SEP '\\'
#define OS_MKDIR(p) _mkdir(p)
#else
#include <dirent.h>
#include <unistd.h>
#define PATH_SEP '/'
#define OS_MKDIR(p) mkdir(p,0755)
#endif

#include "lua.h"
#include "lauxlib.h"
#include "lualib.h"

#if LUA_VERSION_NUM >= 502
#define l_newlib(L,r) luaL_newlib(L,r)
#else
#define l_newlib(L,r) luaL_register(L,"json",r)
#endif

#if LUA_VERSION_NUM >= 503
#define l_isinteger(L,i) lua_isinteger(L,i)
#else
#define l_isinteger(L,i) 0
#endif

#define JSON_MAX_DEPTH  512
#define JSON_BUF_INIT   4096
#define JSON_NULL_KEY   "json.null.sentinel"

typedef struct {
    char   *data;
    size_t  len;
    size_t  cap;
} Buf;

typedef struct {
    const char *s;
    size_t      len;
    size_t      pos;
    int         superstring;
    int         strict;
} Parser;

static int buf_init(Buf *b) {
    b->data = (char *)malloc(JSON_BUF_INIT);
    if (!b->data) return 0;
    b->len = 0; b->cap = JSON_BUF_INIT; return 1;
}
static void buf_free(Buf *b) { if (b->data) free(b->data); b->data=NULL; b->len=0; b->cap=0; }
static int buf_grow(Buf *b, size_t need) {
    size_t nc=b->cap; while(nc<b->len+need)nc*=2;
    if(nc==b->cap)return 1;
    char *nd=(char*)realloc(b->data,nc); if(!nd)return 0;
    b->data=nd; b->cap=nc; return 1;
}
static int buf_push(Buf *b, const char *s, size_t n) {
    if(!buf_grow(b,n))return 0; memcpy(b->data+b->len,s,n); b->len+=n; return 1;
}
static int buf_pushc(Buf *b, char c) { return buf_push(b,&c,1); }
static int buf_pushs(Buf *b, const char *s) { return buf_push(b,s,strlen(s)); }

static void push_null(lua_State *L) {
    lua_getfield(L, LUA_REGISTRYINDEX, JSON_NULL_KEY);
}

static int is_null(lua_State *L, int idx) {
    lua_getfield(L, LUA_REGISTRYINDEX, JSON_NULL_KEY);
    int eq = lua_rawequal(L, idx, -1);
    lua_pop(L, 1);
    return eq;
}

static const char *HEX = "0123456789abcdef";

static int enc_string(Buf *b, const char *s, size_t n) {
    if(!buf_pushc(b,'"'))return 0;
    for(size_t i=0;i<n;i++){
        unsigned char c=(unsigned char)s[i];
        if      (c=='"')  {if(!buf_pushs(b,"\\\""))return 0;}
        else if (c=='\\') {if(!buf_pushs(b,"\\\\"))return 0;}
        else if (c=='\b') {if(!buf_pushs(b,"\\b")) return 0;}
        else if (c=='\f') {if(!buf_pushs(b,"\\f")) return 0;}
        else if (c=='\n') {if(!buf_pushs(b,"\\n")) return 0;}
        else if (c=='\r') {if(!buf_pushs(b,"\\r")) return 0;}
        else if (c=='\t') {if(!buf_pushs(b,"\\t")) return 0;}
        else if (c<0x20)  {char e[7];e[0]='\\';e[1]='u';e[2]='0';e[3]='0';e[4]=HEX[(c>>4)&0xF];e[5]=HEX[c&0xF];e[6]='\0';if(!buf_pushs(b,e))return 0;}
        else              {if(!buf_pushc(b,(char)c))return 0;}
    }
    return buf_pushc(b,'"');
}

static int enc_val(lua_State *L, Buf *b, int idx, int depth, int pretty, int indent, int sort_keys, int superstring);

static int enc_table(lua_State *L, Buf *b, int idx, int depth, int pretty, int indent, int sort_keys, int superstring) {
    if(depth>JSON_MAX_DEPTH){buf_free(b);luaL_error(L,"json.encode: max depth exceeded");return 0;}
    int is_arr=1; lua_Integer maxn=0,count=0;
    lua_pushnil(L);
    while(lua_next(L,idx)!=0){
        if(lua_type(L,-2)==LUA_TNUMBER){lua_Number kn=lua_tonumber(L,-2);lua_Integer ki=(lua_Integer)kn;if((lua_Number)ki==kn&&ki>=1){if(ki>maxn)maxn=ki;count++;lua_pop(L,1);continue;}}
        is_arr=0;lua_pop(L,2);break;
    }
    if(is_arr&&maxn!=count)is_arr=0;
    char po[JSON_MAX_DEPTH*4+1],pi[JSON_MAX_DEPTH*4+1];
    if(pretty){
        int ol=indent*depth,il=indent*(depth+1);
        if(ol>(int)(sizeof(po)-1))ol=(int)(sizeof(po)-1);
        if(il>(int)(sizeof(pi)-1))il=(int)(sizeof(pi)-1);
        memset(po,' ',ol);po[ol]='\0';memset(pi,' ',il);pi[il]='\0';
    }
    if(is_arr){
        buf_pushc(b,'[');int first=1;
        for(lua_Integer i=1;i<=maxn;i++){
            if(!first&&!buf_pushc(b,','))return 0;
            if(pretty){buf_pushc(b,'\n');buf_pushs(b,pi);}
            lua_rawgeti(L,idx,(int)i);
            if(lua_isnil(L,-1)){lua_pop(L,1);buf_pushs(b,"null");}
            else{enc_val(L,b,lua_gettop(L),depth+1,pretty,indent,sort_keys,superstring);lua_pop(L,1);}
            first=0;
        }
        if(pretty&&!first){buf_pushc(b,'\n');buf_pushs(b,po);}
        return buf_pushc(b,']');
    }
    int nk=0;lua_pushnil(L);
    while(lua_next(L,idx)!=0){int kt=lua_type(L,-2);if(kt==LUA_TSTRING||kt==LUA_TNUMBER)nk++;lua_pop(L,1);}
    buf_pushc(b,'{');
    if(nk==0)return buf_pushc(b,'}');
    const char **keys=(const char**)malloc(sizeof(const char*)*nk);
    char **nks=(char**)malloc(sizeof(char*)*nk);
    if(!keys||!nks){if(keys)free(keys);if(nks)free(nks);buf_free(b);luaL_error(L,"json: out of memory");return 0;}
    memset(nks,0,sizeof(char*)*nk);int ns=0;
    lua_pushnil(L);
    while(lua_next(L,idx)!=0){
        int kt=lua_type(L,-2);
        if(kt==LUA_TSTRING){keys[ns]=lua_tostring(L,-2);nks[ns]=NULL;ns++;}
        else if(kt==LUA_TNUMBER){char tmp[64];snprintf(tmp,sizeof(tmp),"%g",(double)lua_tonumber(L,-2));nks[ns]=strdup(tmp);keys[ns]=nks[ns];ns++;}
        lua_pop(L,1);
    }
    if(sort_keys)for(int a=0;a<ns-1;a++)for(int bb=a+1;bb<ns;bb++)if(strcmp(keys[a],keys[bb])>0){const char*tk=keys[a];keys[a]=keys[bb];keys[bb]=tk;char*tn=nks[a];nks[a]=nks[bb];nks[bb]=tn;}
    int first=1;
    for(int ki=0;ki<ns;ki++){
        lua_pushstring(L,keys[ki]);lua_gettable(L,idx);
        if(lua_isnil(L,-1)){lua_pop(L,1);continue;}
        if(!first&&!buf_pushc(b,','))goto cleanup;
        if(pretty){buf_pushc(b,'\n');buf_pushs(b,pi);}
        enc_string(b,keys[ki],strlen(keys[ki]));
        pretty?buf_pushs(b,": "):buf_pushc(b,':');
        enc_val(L,b,lua_gettop(L),depth+1,pretty,indent,sort_keys,superstring);
        lua_pop(L,1);first=0;
    }
    if(pretty&&!first){buf_pushc(b,'\n');buf_pushs(b,po);}
    buf_pushc(b,'}');
    for(int i=0;i<ns;i++)if(nks[i])free(nks[i]);free(keys);free(nks);return 1;
cleanup:
    for(int i=0;i<ns;i++)if(nks[i])free(nks[i]);free(keys);free(nks);return 0;
}

static int enc_val(lua_State *L, Buf *b, int idx, int depth, int pretty, int indent, int sort_keys, int superstring) {
    if(is_null(L,idx)) return buf_pushs(b,"null");
    switch(lua_type(L,idx)){
        case LUA_TNIL:     return buf_pushs(b,"null");
        case LUA_TBOOLEAN: {int v=lua_toboolean(L,idx);return buf_pushs(b,superstring?(v?"yes":"no"):(v?"true":"false"));}
        case LUA_TNUMBER: {
#if LUA_VERSION_NUM>=503
            if(l_isinteger(L,idx)){char t[32];snprintf(t,sizeof(t),"%lld",(long long)lua_tointeger(L,idx));return buf_pushs(b,t);}
#endif
            double n=(double)lua_tonumber(L,idx);
            if(isnan(n)||isinf(n))return buf_pushs(b,"null");
            if(n==(long long)n&&fabs(n)<1e15){char t[32];snprintf(t,sizeof(t),"%lld",(long long)n);return buf_pushs(b,t);}
            char t[64];snprintf(t,sizeof(t),"%.17g",n);return buf_pushs(b,t);
        }
        case LUA_TSTRING: {size_t n;const char *s=lua_tolstring(L,idx,&n);return enc_string(b,s,n);}
        case LUA_TTABLE:  {lua_pushvalue(L,idx);return enc_table(L,b,lua_gettop(L),depth,pretty,indent,sort_keys,superstring);}
        default: buf_free(b);luaL_error(L,"json.encode: unsupported type: %s",lua_typename(L,lua_type(L,idx)));return 0;
    }
}

static void skip_ws(Parser *p){while(p->pos<p->len){char c=p->s[p->pos];if(c==' '||c=='\t'||c=='\n'||c=='\r')p->pos++;else break;}}
static int parse_val(lua_State *L, Parser *p, int depth);

static int parse_str(lua_State *L, Parser *p){
    p->pos++;luaL_Buffer lb;luaL_buffinit(L,&lb);
    while(p->pos<p->len){
        unsigned char c=(unsigned char)p->s[p->pos];
        if(c=='"'){p->pos++;luaL_pushresult(&lb);return 1;}
        if(c=='\\'){
            p->pos++;if(p->pos>=p->len)luaL_error(L,"json.decode: unterminated escape");
            unsigned char e=(unsigned char)p->s[p->pos];
            switch(e){
                case '"': luaL_addchar(&lb,'"'); p->pos++;break;
                case '\\':luaL_addchar(&lb,'\\');p->pos++;break;
                case '/': luaL_addchar(&lb,'/'); p->pos++;break;
                case 'b': luaL_addchar(&lb,'\b');p->pos++;break;
                case 'f': luaL_addchar(&lb,'\f');p->pos++;break;
                case 'n': luaL_addchar(&lb,'\n');p->pos++;break;
                case 'r': luaL_addchar(&lb,'\r');p->pos++;break;
                case 't': luaL_addchar(&lb,'\t');p->pos++;break;
                case 'u':{
                    p->pos++;if(p->pos+4>p->len)luaL_error(L,"json.decode: invalid \\u at %d",(int)p->pos);
                    char hb[5];memcpy(hb,p->s+p->pos,4);hb[4]='\0';unsigned int cp=0;
                    if(sscanf(hb,"%x",&cp)!=1)luaL_error(L,"json.decode: invalid \\u at %d",(int)p->pos);
                    p->pos+=4;
                    if(cp>=0xD800&&cp<=0xDBFF){
                        if(p->pos+6>p->len||p->s[p->pos]!='\\'||p->s[p->pos+1]!='u')luaL_error(L,"json.decode: missing low surrogate at %d",(int)p->pos);
                        p->pos+=2;char hb2[5];memcpy(hb2,p->s+p->pos,4);hb2[4]='\0';unsigned int low=0;
                        if(sscanf(hb2,"%x",&low)!=1||low<0xDC00||low>0xDFFF)luaL_error(L,"json.decode: invalid low surrogate at %d",(int)p->pos);
                        p->pos+=4;cp=0x10000+(cp-0xD800)*0x400+(low-0xDC00);
                    }
                    if(cp<0x80){luaL_addchar(&lb,(char)cp);}
                    else if(cp<0x800){luaL_addchar(&lb,(char)(0xC0|(cp>>6)));luaL_addchar(&lb,(char)(0x80|(cp&0x3F)));}
                    else if(cp<0x10000){luaL_addchar(&lb,(char)(0xE0|(cp>>12)));luaL_addchar(&lb,(char)(0x80|((cp>>6)&0x3F)));luaL_addchar(&lb,(char)(0x80|(cp&0x3F)));}
                    else{luaL_addchar(&lb,(char)(0xF0|(cp>>18)));luaL_addchar(&lb,(char)(0x80|((cp>>12)&0x3F)));luaL_addchar(&lb,(char)(0x80|((cp>>6)&0x3F)));luaL_addchar(&lb,(char)(0x80|(cp&0x3F)));}
                    break;
                }
                default:luaL_error(L,"json.decode: invalid escape \\%c at %d",e,(int)p->pos);
            }
        } else {luaL_addchar(&lb,(char)c);p->pos++;}
    }
    luaL_error(L,"json.decode: unterminated string");return 0;
}

static int parse_num(lua_State *L, Parser *p){
    size_t start=p->pos;if(p->s[p->pos]=='-')p->pos++;
    size_t ns=p->pos;while(p->pos<p->len&&p->s[p->pos]>='0'&&p->s[p->pos]<='9')p->pos++;
    if(p->strict&&(p->pos-ns)>1&&p->s[ns]=='0')luaL_error(L,"json.decode: leading zeros not allowed at %d",(int)start);
    int is_f=0;
    if(p->pos<p->len&&p->s[p->pos]=='.'){is_f=1;p->pos++;while(p->pos<p->len&&p->s[p->pos]>='0'&&p->s[p->pos]<='9')p->pos++;}
    if(p->pos<p->len&&(p->s[p->pos]=='e'||p->s[p->pos]=='E')){is_f=1;p->pos++;if(p->pos<p->len&&(p->s[p->pos]=='+'||p->s[p->pos]=='-'))p->pos++;while(p->pos<p->len&&p->s[p->pos]>='0'&&p->s[p->pos]<='9')p->pos++;}
    char tmp[64];size_t nl=p->pos-start;if(nl>=sizeof(tmp))luaL_error(L,"json.decode: number too long");
    memcpy(tmp,p->s+start,nl);tmp[nl]='\0';
    if(is_f)lua_pushnumber(L,(lua_Number)atof(tmp));else lua_pushinteger(L,(lua_Integer)atoll(tmp));
    return 1;
}

static int parse_arr(lua_State *L, Parser *p, int depth){
    if(depth>JSON_MAX_DEPTH)luaL_error(L,"json.decode: max depth exceeded");
    p->pos++;lua_newtable(L);int idx=1;skip_ws(p);
    if(p->pos<p->len&&p->s[p->pos]==']'){p->pos++;return 1;}
    while(1){
        skip_ws(p);parse_val(L,p,depth+1);lua_rawseti(L,-2,idx++);skip_ws(p);
        if(p->pos>=p->len)luaL_error(L,"json.decode: unterminated array");
        char c=p->s[p->pos];if(c==']'){p->pos++;return 1;}else if(c==',')p->pos++;else luaL_error(L,"json.decode: expected , or ] at %d",(int)p->pos);
    }
}

static int parse_obj(lua_State *L, Parser *p, int depth){
    if(depth>JSON_MAX_DEPTH)luaL_error(L,"json.decode: max depth exceeded");
    p->pos++;lua_newtable(L);skip_ws(p);
    if(p->pos<p->len&&p->s[p->pos]=='}'){p->pos++;return 1;}
    while(1){
        skip_ws(p);if(p->pos>=p->len||p->s[p->pos]!='"')luaL_error(L,"json.decode: expected key at %d",(int)p->pos);
        parse_str(L,p);
        if(p->strict){lua_pushvalue(L,-1);lua_gettable(L,-3);if(!lua_isnil(L,-1)){const char*dk=lua_tostring(L,-2);luaL_error(L,"json.decode: duplicate key \"%s\"",dk);}lua_pop(L,1);}
        skip_ws(p);if(p->pos>=p->len||p->s[p->pos]!=':')luaL_error(L,"json.decode: expected : at %d",(int)p->pos);
        p->pos++;skip_ws(p);parse_val(L,p,depth+1);lua_settable(L,-3);skip_ws(p);
        if(p->pos>=p->len)luaL_error(L,"json.decode: unterminated object");
        char c=p->s[p->pos];if(c=='}'){p->pos++;return 1;}else if(c==',')p->pos++;else luaL_error(L,"json.decode: expected , or } at %d",(int)p->pos);
    }
}

static int parse_val(lua_State *L, Parser *p, int depth){
    skip_ws(p);if(p->pos>=p->len)luaL_error(L,"json.decode: unexpected end of input");
    char c=p->s[p->pos];
    if(c=='"')return parse_str(L,p);
    if(c=='{')return parse_obj(L,p,depth);
    if(c=='[')return parse_arr(L,p,depth);
    if(c=='t'&&p->pos+4<=p->len&&memcmp(p->s+p->pos,"true",4)==0){p->pos+=4;lua_pushboolean(L,1);return 1;}
    if(c=='f'&&p->pos+5<=p->len&&memcmp(p->s+p->pos,"false",5)==0){p->pos+=5;lua_pushboolean(L,0);return 1;}
    if(c=='n'&&p->pos+4<=p->len&&memcmp(p->s+p->pos,"null",4)==0){p->pos+=4;push_null(L);return 1;}
    if(p->superstring){
        if(c=='y'&&p->pos+3<=p->len&&memcmp(p->s+p->pos,"yes",3)==0){p->pos+=3;lua_pushboolean(L,1);return 1;}
        if(c=='n'&&p->pos+2<=p->len&&memcmp(p->s+p->pos,"no",2)==0){p->pos+=2;lua_pushboolean(L,0);return 1;}
    }
    if(c=='-'||(c>='0'&&c<='9'))return parse_num(L,p);
    luaL_error(L,"json.decode: unexpected character '%c' at %d",c,(int)p->pos);return 0;
}

static int l_encode(lua_State *L){
    luaL_checkany(L,1);
    int pretty=0,indent=2,sort_keys=0,superstring=0;
    if(lua_gettop(L)>=2&&lua_istable(L,2)){
        lua_getfield(L,2,"pretty");     if(!lua_isnil(L,-1))pretty=lua_toboolean(L,-1);        lua_pop(L,1);
        lua_getfield(L,2,"indent");     if(lua_isnumber(L,-1))indent=(int)lua_tonumber(L,-1);  lua_pop(L,1);
        lua_getfield(L,2,"sort_keys");  if(!lua_isnil(L,-1))sort_keys=lua_toboolean(L,-1);     lua_pop(L,1);
        lua_getfield(L,2,"superstring");if(!lua_isnil(L,-1))superstring=lua_toboolean(L,-1);   lua_pop(L,1);
    }
    Buf b;if(!buf_init(&b))luaL_error(L,"json.encode: out of memory");
    enc_val(L,&b,1,0,pretty,indent,sort_keys,superstring);
    lua_pushlstring(L,b.data,b.len);buf_free(&b);return 1;
}

static int l_decode(lua_State *L){
    size_t len;const char *s=luaL_checklstring(L,1,&len);
    Parser p;p.s=s;p.len=len;p.pos=0;p.superstring=0;p.strict=0;
    if(lua_gettop(L)>=2&&lua_istable(L,2)){
        lua_getfield(L,2,"superstring");if(!lua_isnil(L,-1))p.superstring=lua_toboolean(L,-1);lua_pop(L,1);
        lua_getfield(L,2,"strict");     if(!lua_isnil(L,-1))p.strict=lua_toboolean(L,-1);     lua_pop(L,1);
    }
    parse_val(L,&p,0);skip_ws(&p);
    if(p.pos<p.len)luaL_error(L,"json.decode: trailing garbage at %d",(int)p.pos);
    return 1;
}

static int l_validate(lua_State *L){
    size_t len;const char *s=luaL_checklstring(L,1,&len);
    Parser p;p.s=s;p.len=len;p.pos=0;p.superstring=0;p.strict=1;
    int ok=lua_pcall(L,0,0,0);
    if(ok==0){lua_pushboolean(L,1);return 1;}
    lua_pushboolean(L,0);lua_pushvalue(L,-2);return 2;
}

static int l_pf_exists(lua_State *L){
    const char *path=luaL_checkstring(L,1);
    FILE *f=fopen(path,"r");
    lua_pushboolean(L,f!=NULL);if(f)fclose(f);return 1;
}
static int l_pf_read(lua_State *L){
    const char *path=luaL_checkstring(L,1);
    FILE *f=fopen(path,"rb");
    if(!f)luaL_error(L,"json.read: cannot open '%s': %s",path,strerror(errno));
    fseek(f,0,SEEK_END);long sz=ftell(f);fseek(f,0,SEEK_SET);
    char *buf=(char*)malloc(sz+1);if(!buf){fclose(f);luaL_error(L,"json.read: out of memory");}
    fread(buf,1,sz,f);fclose(f);buf[sz]='\0';
    lua_pushlstring(L,buf,sz);free(buf);return 1;
}
static int l_pf_write(lua_State *L){
    const char *path=luaL_checkstring(L,1);
    size_t len;const char *data=luaL_checklstring(L,2,&len);
    FILE *f=fopen(path,"wb");if(!f)luaL_error(L,"json.write: cannot open '%s': %s",path,strerror(errno));
    fwrite(data,1,len,f);fclose(f);return 0;
}
static int l_pf_append(lua_State *L){
    const char *path=luaL_checkstring(L,1);
    size_t len;const char *data=luaL_checklstring(L,2,&len);
    FILE *f=fopen(path,"ab");if(!f)luaL_error(L,"json.append: cannot open '%s': %s",path,strerror(errno));
    fwrite(data,1,len,f);fclose(f);return 0;
}
static int l_pf_delete(lua_State *L){
    const char *path=luaL_checkstring(L,1);
    if(remove(path)!=0)luaL_error(L,"json.delete: cannot delete '%s': %s",path,strerror(errno));
    return 0;
}
static int l_pf_rename(lua_State *L){
    const char *a=luaL_checkstring(L,1),*b=luaL_checkstring(L,2);
    if(rename(a,b)!=0)luaL_error(L,"json.rename: cannot rename '%s': %s",a,strerror(errno));
    return 0;
}
static int l_pf_copy(lua_State *L){
    const char *src=luaL_checkstring(L,1),*dst=luaL_checkstring(L,2);
    FILE *fs=fopen(src,"rb");if(!fs)luaL_error(L,"json.copy: cannot open '%s': %s",src,strerror(errno));
    FILE *fd=fopen(dst,"wb");if(!fd){fclose(fs);luaL_error(L,"json.copy: cannot open '%s': %s",dst,strerror(errno));}
    char buf[8192];size_t n;while((n=fread(buf,1,sizeof(buf),fs))>0)fwrite(buf,1,n,fd);
    fclose(fs);fclose(fd);return 0;
}
static int l_pf_size(lua_State *L){
    const char *path=luaL_checkstring(L,1);
    FILE *f=fopen(path,"rb");if(!f)luaL_error(L,"json.size: cannot open '%s': %s",path,strerror(errno));
    fseek(f,0,SEEK_END);long sz=ftell(f);fclose(f);
    lua_pushinteger(L,(lua_Integer)sz);return 1;
}
static int l_pf_mkdir(lua_State *L){
    const char *path=luaL_checkstring(L,1);
    char tmp[4096];strncpy(tmp,path,sizeof(tmp)-1);tmp[sizeof(tmp)-1]='\0';
    for(char *p=tmp+1;*p;p++){if(*p=='/'||*p=='\\'){char sv=*p;*p='\0';OS_MKDIR(tmp);*p=sv;}}
    OS_MKDIR(tmp);return 0;
}
static int l_pf_list(lua_State *L){
    const char *path=luaL_checkstring(L,1);
    lua_newtable(L);int idx=1;
#ifdef _WIN32
    char pat[4096];snprintf(pat,sizeof(pat),"%s\\*",path);
    WIN32_FIND_DATA fd;HANDLE h=FindFirstFile(pat,&fd);
    if(h==INVALID_HANDLE_VALUE)luaL_error(L,"json.list: cannot list '%s'",path);
    do{if(strcmp(fd.cFileName,".")!=0&&strcmp(fd.cFileName,"..")!=0){lua_pushstring(L,fd.cFileName);lua_rawseti(L,-2,idx++);}}while(FindNextFile(h,&fd));
    FindClose(h);
#else
    DIR *d=opendir(path);if(!d)luaL_error(L,"json.list: cannot list '%s': %s",path,strerror(errno));
    struct dirent *e;while((e=readdir(d))!=NULL){if(strcmp(e->d_name,".")!=0&&strcmp(e->d_name,"..")!=0){lua_pushstring(L,e->d_name);lua_rawseti(L,-2,idx++);}}
    closedir(d);
#endif
    return 1;
}
static int l_pf_is_dir(lua_State *L){
    const char *path=luaL_checkstring(L,1);struct stat st;
    if(stat(path,&st)!=0){lua_pushboolean(L,0);return 1;}
    lua_pushboolean(L,S_ISDIR(st.st_mode));return 1;
}
static int l_pf_is_file(lua_State *L){
    const char *path=luaL_checkstring(L,1);struct stat st;
    if(stat(path,&st)!=0){lua_pushboolean(L,0);return 1;}
    lua_pushboolean(L,S_ISREG(st.st_mode));return 1;
}
static int l_pf_read_lines(lua_State *L){
    const char *path=luaL_checkstring(L,1);
    FILE *f=fopen(path,"r");if(!f)luaL_error(L,"json.read_lines: cannot open '%s': %s",path,strerror(errno));
    lua_newtable(L);int idx=1;char line[65536];
    while(fgets(line,sizeof(line),f)){size_t len=strlen(line);while(len>0&&(line[len-1]=='\n'||line[len-1]=='\r'))len--;lua_pushlstring(L,line,len);lua_rawseti(L,-2,idx++);}
    fclose(f);return 1;
}
static int l_pf_join(lua_State *L){
    int n=lua_gettop(L);luaL_Buffer b;luaL_buffinit(L,&b);
    for(int i=1;i<=n;i++){if(i>1)luaL_addchar(&b,PATH_SEP);luaL_addstring(&b,luaL_checkstring(L,i));}
    luaL_pushresult(&b);return 1;
}
static int l_pf_basename(lua_State *L){
    const char *path=luaL_checkstring(L,1);const char *s=path,*p=path;
    while(*p){if(*p=='/'||*p=='\\')s=p+1;p++;}lua_pushstring(L,s);return 1;
}
static int l_pf_dirname(lua_State *L){
    const char *path=luaL_checkstring(L,1);const char *last=NULL;
    for(const char *p=path;*p;p++)if(*p=='/'||*p=='\\')last=p;
    if(!last){lua_pushliteral(L,".");return 1;}
    lua_pushlstring(L,path,last-path);return 1;
}
static int l_pf_extension(lua_State *L){
    const char *path=luaL_checkstring(L,1);const char *dot=NULL;
    for(const char *p=path;*p;p++){if(*p=='.'&&*(p+1)!='\0')dot=p+1;else if(*p=='/'||*p=='\\')dot=NULL;}
    lua_pushstring(L,dot?dot:"");return 1;
}
static int l_pf_stem(lua_State *L){
    const char *path=luaL_checkstring(L,1);const char *base=path;
    for(const char *p=path;*p;p++)if(*p=='/'||*p=='\\')base=p+1;
    const char *dot=NULL;for(const char *p=base;*p;p++)if(*p=='.')dot=p;
    if(!dot)lua_pushstring(L,base);else lua_pushlstring(L,base,dot-base);return 1;
}

static int l_encode_file(lua_State *L){
    const char *path=luaL_checkstring(L,1);luaL_checkany(L,2);
    int pretty=0,indent=2,sort_keys=0,superstring=0;
    if(lua_gettop(L)>=3&&lua_istable(L,3)){
        lua_getfield(L,3,"pretty");     if(!lua_isnil(L,-1))pretty=lua_toboolean(L,-1);       lua_pop(L,1);
        lua_getfield(L,3,"indent");     if(lua_isnumber(L,-1))indent=(int)lua_tonumber(L,-1); lua_pop(L,1);
        lua_getfield(L,3,"sort_keys");  if(!lua_isnil(L,-1))sort_keys=lua_toboolean(L,-1);    lua_pop(L,1);
        lua_getfield(L,3,"superstring");if(!lua_isnil(L,-1))superstring=lua_toboolean(L,-1);  lua_pop(L,1);
    }
    Buf b;if(!buf_init(&b))luaL_error(L,"json.encode_file: out of memory");
    enc_val(L,&b,2,0,pretty,indent,sort_keys,superstring);
    FILE *f=fopen(path,"wb");if(!f){buf_free(&b);luaL_error(L,"json.encode_file: cannot open '%s': %s",path,strerror(errno));}
    fwrite(b.data,1,b.len,f);fclose(f);buf_free(&b);return 0;
}

static int l_decode_file(lua_State *L){
    const char *path=luaL_checkstring(L,1);
    FILE *f=fopen(path,"rb");if(!f)luaL_error(L,"json.decode_file: cannot open '%s': %s",path,strerror(errno));
    fseek(f,0,SEEK_END);long sz=ftell(f);fseek(f,0,SEEK_SET);
    char *buf=(char*)malloc(sz+1);if(!buf){fclose(f);luaL_error(L,"json.decode_file: out of memory");}
    fread(buf,1,sz,f);fclose(f);buf[sz]='\0';
    Parser p;p.s=buf;p.len=sz;p.pos=0;p.superstring=0;p.strict=0;
    if(lua_gettop(L)>=2&&lua_istable(L,2)){
        lua_getfield(L,2,"superstring");if(!lua_isnil(L,-1))p.superstring=lua_toboolean(L,-1);lua_pop(L,1);
        lua_getfield(L,2,"strict");     if(!lua_isnil(L,-1))p.strict=lua_toboolean(L,-1);     lua_pop(L,1);
    }
    parse_val(L,&p,0);free(buf);return 1;
}

static int l_append_file(lua_State *L){
    const char *path=luaL_checkstring(L,1);luaL_checkany(L,2);
    Buf b;if(!buf_init(&b))luaL_error(L,"json.append_file: out of memory");
    enc_val(L,&b,2,0,0,2,0,0);buf_pushc(&b,'\n');
    FILE *f=fopen(path,"ab");if(!f){buf_free(&b);luaL_error(L,"json.append_file: cannot open '%s': %s",path,strerror(errno));}
    fwrite(b.data,1,b.len,f);fclose(f);buf_free(&b);return 0;
}

static int l_decode_lines(lua_State *L){
    const char *path=luaL_checkstring(L,1);
    FILE *f=fopen(path,"r");if(!f)luaL_error(L,"json.decode_lines: cannot open '%s': %s",path,strerror(errno));
    lua_newtable(L);int idx=1;char line[65536];
    while(fgets(line,sizeof(line),f)){
        size_t len=strlen(line);while(len>0&&(line[len-1]=='\n'||line[len-1]=='\r'))len--;
        if(len==0)continue;
        Parser p;p.s=line;p.len=len;p.pos=0;p.superstring=0;p.strict=0;
        parse_val(L,&p,0);lua_rawseti(L,-2,idx++);
    }
    fclose(f);return 1;
}

static int l_null_tostring(lua_State *L){lua_pushliteral(L,"null");return 1;}

static const luaL_Reg json_lib[] = {
    {"encode",       l_encode},
    {"decode",       l_decode},
    {"validate",     l_validate},
    {"encode_file",  l_encode_file},
    {"decode_file",  l_decode_file},
    {"append_file",  l_append_file},
    {"decode_lines", l_decode_lines},
    {"exists",       l_pf_exists},
    {"read",         l_pf_read},
    {"write",        l_pf_write},
    {"append",       l_pf_append},
    {"delete",       l_pf_delete},
    {"rename",       l_pf_rename},
    {"copy",         l_pf_copy},
    {"size",         l_pf_size},
    {"mkdir",        l_pf_mkdir},
    {"list",         l_pf_list},
    {"is_dir",       l_pf_is_dir},
    {"is_file",      l_pf_is_file},
    {"read_lines",   l_pf_read_lines},
    {"join",         l_pf_join},
    {"basename",     l_pf_basename},
    {"dirname",      l_pf_dirname},
    {"extension",    l_pf_extension},
    {"stem",         l_pf_stem},
    {NULL, NULL}
};

#if LUA_VERSION_NUM >= 502
int luaopen_json(lua_State *L) {
    luaL_newlib(L, json_lib);
    lua_newtable(L);
    lua_newtable(L);
    lua_pushcfunction(L, l_null_tostring);
    lua_setfield(L, -2, "__tostring");
    lua_pushliteral(L, "json.null");
    lua_setfield(L, -2, "__name");
    lua_setmetatable(L, -2);
    lua_pushvalue(L, -1);
    lua_setfield(L, LUA_REGISTRYINDEX, JSON_NULL_KEY);
    lua_setfield(L, -2, "null");
    return 1;
}
#else
int luaopen_json(lua_State *L) {
    luaL_register(L, "json", json_lib);
    lua_newtable(L);
    lua_newtable(L);
    lua_pushcfunction(L, l_null_tostring);
    lua_setfield(L, -2, "__tostring");
    lua_setmetatable(L, -2);
    lua_pushvalue(L, -1);
    lua_setfield(L, LUA_REGISTRYINDEX, JSON_NULL_KEY);
    lua_setfield(L, -2, "null");
    return 1;
}
#endif
