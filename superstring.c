#include "cluajit.h"

static void skip_ws(Parser *p){while(p->pos<p->len){char c=p->s[p->pos];if(c==' '||c=='\t'||c=='\n'||c=='\r')p->pos++;else break;}}
static int parse_val(lua_State *L, Parser *p, int depth);

static int parse_str(lua_State *L, Parser *p){
    p->pos++;luaL_Buffer lb;luaL_buffinit(L,&lb);
    while(p->pos<p->len){
        unsigned char c=(unsigned char)p->s[p->pos];
        if(c=='"'){p->pos++;luaL_pushresult(&lb);return 1;}
        if(c=='\\'){
            p->pos++;if(p->pos>=p->len)luaL_error(L,"superstring.decode: unterminated escape");
            unsigned char e=(unsigned char)p->s[p->pos];
            switch(e){
                case '"': luaL_addchar(&lb,'"'); p->pos++;break;
                case '\\'luaL_addchar(&lb,'\\');p->pos++;break;
                case '/': luaL_addchar(&lb,'/'); p->pos++;break;
                case 'b': luaL_addchar(&lb,'\b');p->pos++;break;
                case 'f': luaL_addchar(&lb,'\f');p->pos++;break;
                case 'n': luaL_addchar(&lb,'\n');p->pos++;break;
                case 'r': luaL_addchar(&lb,'\r');p->pos++;break;
                case 't': luaL_addchar(&lb,'\t');p->pos++;break;
                case 'u':{
                    p->pos++;if(p->pos+4>p->len)luaL_error(L,"superstring.decode: invalid \\u at %d",(int)p->pos);
                    char hb[5];memcpy(hb,p->s+p->pos,4);hb[4]='\0';unsigned int cp=0;
                    if(sscanf(hb,"%x",&cp)!=1)luaL_error(L,"superstring.decode: invalid \\u at %d",(int)p->pos);
                    p->pos+=4;
                    if(cp>=0xD800&&cp<=0xDBFF){
                        if(p->pos+6>p->len||p->s[p->pos]!='\\'||p->s[p->pos+1]!='u')luaL_error(L,"superstring.decode: missing low surrogate at %d",(int)p->pos);
                        p->pos+=2;char hb2[5];memcpy(hb2,p->s+p->pos,4);hb2[4]='\0';unsigned int low=0;
                        if(sscanf(hb2,"%x",&low)!=1||low<0xDC00||low>0xDFFF)luaL_error(L,"superstring.decode: invalid low surrogate at %d",(int)p->pos);
                        p->pos+=4;cp=0x10000+(cp-0xD800)*0x400+(low-0xDC00);
                    }
                    if(cp<0x80){luaL_addchar(&lb,(char)cp);}
                    else if(cp<0x800){luaL_addchar(&lb,(char)(0xC0|(cp>>6)));luaL_addchar(&lb,(char)(0x80|(cp&0x3F)));}
                    else if(cp<0x10000){luaL_addchar(&lb,(char)(0xE0|(cp>>12)));luaL_addchar(&lb,(char)(0x80|((cp>>6)&0x3F)));luaL_addchar(&lb,(char)(0x80|(cp&0x3F)));}
                    else{luaL_addchar(&lb,(char)(0xF0|(cp>>18)));luaL_addchar(&lb,(char)(0x80|((cp>>12)&0x3F)));luaL_addchar(&lb,(char)(0x80|((cp>>6)&0x3F)));luaL_addchar(&lb,(char)(0x80|(cp&0x3F)));}
                    break;
                }
                default:luaL_error(L,"superstring.decode: invalid escape \\%c at %d",e,(int)p->pos);
            }
        } else {luaL_addchar(&lb,(char)c);p->pos++;}
    }
    luaL_error(L,"superstring.decode: unterminated string");return 0;
}

static int parse_num(lua_State *L, Parser *p){
    size_t start=p->pos;if(p->s[p->pos]=='-')p->pos++;
    size_t ns=p->pos;while(p->pos<p->len&&p->s[p->pos]>='0'&&p->s[p->pos]<='9')p->pos++;
    if(p->strict&&(p->pos-ns)>1&&p->s[ns]=='0')luaL_error(L,"superstring.decode: leading zeros not allowed at %d",(int)start);
    int is_f=0;
    if(p->pos<p->len&&p->s[p->pos]=='.'){is_f=1;p->pos++;while(p->pos<p->len&&p->s[p->pos]>='0'&&p->s[p->pos]<='9')p->pos++;}
    if(p->pos<p->len&&(p->s[p->pos]=='e'||p->s[p->pos]=='E')){is_f=1;p->pos++;if(p->pos<p->len&&(p->s[p->pos]=='+'||p->s[p->pos]=='-'))p->pos++;while(p->pos<p->len&&p->s[p->pos]>='0'&&p->s[p->pos]<='9')p->pos++;}
    char tmp[64];size_t nl=p->pos-start;if(nl>=sizeof(tmp))luaL_error(L,"superstring.decode: number too long");
    memcpy(tmp,p->s+start,nl);tmp[nl]='\0';
    if(is_f)lua_pushnumber(L,(lua_Number)atof(tmp));else lua_pushinteger(L,(lua_Integer)atoll(tmp));
    return 1;
}

static int parse_arr(lua_State *L, Parser *p, int depth){
    if(depth>CLUAJIT_MAX_DEPTH)luaL_error(L,"superstring.decode: max depth exceeded");
    p->pos++;lua_newtable(L);int idx=1;skip_ws(p);
    if(p->pos<p->len&&p->s[p->pos]==']'){p->pos++;return 1;}
    while(1){skip_ws(p);parse_val(L,p,depth+1);lua_rawseti(L,-2,idx++);skip_ws(p);
        if(p->pos>=p->len)luaL_error(L,"superstring.decode: unterminated array");
        char c=p->s[p->pos];if(c==']'){p->pos++;return 1;}else if(c==',')p->pos++;else luaL_error(L,"superstring.decode: expected , or ] at %d",(int)p->pos);}
}

static int parse_obj(lua_State *L, Parser *p, int depth){
    if(depth>CLUAJIT_MAX_DEPTH)luaL_error(L,"superstring.decode: max depth exceeded");
    p->pos++;lua_newtable(L);skip_ws(p);
    if(p->pos<p->len&&p->s[p->pos]=='}'){p->pos++;return 1;}
    while(1){
        skip_ws(p);if(p->pos>=p->len||p->s[p->pos]!='"')luaL_error(L,"superstring.decode: expected key at %d",(int)p->pos);
        parse_str(L,p);
        if(p->strict){lua_pushvalue(L,-1);lua_gettable(L,-3);if(!lua_isnil(L,-1)){const char*dk=lua_tostring(L,-2);luaL_error(L,"superstring.decode: duplicate key \"%s\"",dk);}lua_pop(L,1);}
        skip_ws(p);if(p->pos>=p->len||p->s[p->pos]!=':')luaL_error(L,"superstring.decode: expected : at %d",(int)p->pos);
        p->pos++;skip_ws(p);parse_val(L,p,depth+1);lua_settable(L,-3);skip_ws(p);
        if(p->pos>=p->len)luaL_error(L,"superstring.decode: unterminated object");
        char c=p->s[p->pos];if(c=='}'){p->pos++;return 1;}else if(c==',')p->pos++;else luaL_error(L,"superstring.decode: expected , or } at %d",(int)p->pos);
    }
}

static int parse_val(lua_State *L, Parser *p, int depth){
    skip_ws(p);if(p->pos>=p->len)luaL_error(L,"superstring.decode: unexpected end of input");
    char c=p->s[p->pos];
    if(c=='"')return parse_str(L,p);
    if(c=='{')return parse_obj(L,p,depth);
    if(c=='[')return parse_arr(L,p,depth);
    if(c=='t'&&p->pos+4<=p->len&&memcmp(p->s+p->pos,"true",4)==0){p->pos+=4;lua_pushboolean(L,1);return 1;}
    if(c=='f'&&p->pos+5<=p->len&&memcmp(p->s+p->pos,"false",5)==0){p->pos+=5;lua_pushboolean(L,0);return 1;}
    if(c=='n'&&p->pos+4<=p->len&&memcmp(p->s+p->pos,"null",4)==0){
        p->pos+=4;lua_getglobal(L,"json");
        if(lua_istable(L,-1)){lua_getfield(L,-1,"null");lua_remove(L,-2);}else{lua_pop(L,1);lua_pushnil(L);}
        return 1;
    }
    if(p->superstring){
        if(c=='y'&&p->pos+3<=p->len&&memcmp(p->s+p->pos,"yes",3)==0){p->pos+=3;lua_pushboolean(L,1);return 1;}
        if(c=='n'&&p->pos+2<=p->len&&memcmp(p->s+p->pos,"no",2)==0){p->pos+=2;lua_pushboolean(L,0);return 1;}
    }
    if(c=='-'||(c>='0'&&c<='9'))return parse_num(L,p);
    luaL_error(L,"superstring.decode: unexpected character '%c' at %d",c,(int)p->pos);return 0;
}

static const char *HEX2="0123456789abcdef";
static int ss_enc_str(Buffer *b, const char *s, size_t n){
    if(!buf_pushc(b,'"'))return 0;
    for(size_t i=0;i<n;i++){
        unsigned char c=(unsigned char)s[i];
        if(c=='"'){if(!buf_pushs(b,"\\\""))return 0;}
        else if(c=='\\'){if(!buf_pushs(b,"\\\\"))return 0;}
        else if(c=='\b'){if(!buf_pushs(b,"\\b"))return 0;}
        else if(c=='\f'){if(!buf_pushs(b,"\\f"))return 0;}
        else if(c=='\n'){if(!buf_pushs(b,"\\n"))return 0;}
        else if(c=='\r'){if(!buf_pushs(b,"\\r"))return 0;}
        else if(c=='\t'){if(!buf_pushs(b,"\\t"))return 0;}
        else if(c<0x20){char e[7];e[0]='\\';e[1]='u';e[2]='0';e[3]='0';e[4]=HEX2[(c>>4)&0xF];e[5]=HEX2[c&0xF];e[6]='\0';if(!buf_pushs(b,e))return 0;}
        else{if(!buf_pushc(b,(char)c))return 0;}
    }
    return buf_pushc(b,'"');
}

static int ss_enc_val(lua_State *L, Buffer *b, int idx, int depth, int pretty, int indent, int sort_keys);
static int ss_enc_table(lua_State *L, Buffer *b, int idx, int depth, int pretty, int indent, int sort_keys){
    if(depth>CLUAJIT_MAX_DEPTH){buf_free(b);luaL_error(L,"superstring.encode: max depth exceeded");return 0;}
    int is_arr=1;lua_Integer maxn=0,count=0;
    lua_pushnil(L);
    while(lua_next(L,idx)!=0){
        if(lua_type(L,-2)==LUA_TNUMBER){lua_Number kn=lua_tonumber(L,-2);lua_Integer ki=(lua_Integer)kn;if((lua_Number)ki==kn&&ki>=1){if(ki>maxn)maxn=ki;count++;lua_pop(L,1);continue;}}
        is_arr=0;lua_pop(L,2);break;
    }
    if(is_arr&&maxn!=count)is_arr=0;
    char po[CLUAJIT_MAX_DEPTH*4+1],pi[CLUAJIT_MAX_DEPTH*4+1];
    if(pretty){int ol=indent*depth,il=indent*(depth+1);if(ol>(int)(sizeof(po)-1))ol=(int)(sizeof(po)-1);if(il>(int)(sizeof(pi)-1))il=(int)(sizeof(pi)-1);memset(po,' ',ol);po[ol]='\0';memset(pi,' ',il);pi[il]='\0';}
    if(is_arr){
        buf_pushc(b,'[');int first=1;
        for(lua_Integer i=1;i<=maxn;i++){
            if(!first&&!buf_pushc(b,','))return 0;
            if(pretty){buf_pushc(b,'\n');buf_pushs(b,pi);}
            lua_rawgeti(L,idx,(int)i);
            if(lua_isnil(L,-1)){lua_pop(L,1);buf_pushs(b,"null");}else{ss_enc_val(L,b,lua_gettop(L),depth+1,pretty,indent,sort_keys);lua_pop(L,1);}
            first=0;
        }
        if(pretty&&!first){buf_pushc(b,'\n');buf_pushs(b,po);}
        return buf_pushc(b,']');
    }
    int nk=0;lua_pushnil(L);while(lua_next(L,idx)!=0){int kt=lua_type(L,-2);if(kt==LUA_TSTRING||kt==LUA_TNUMBER)nk++;lua_pop(L,1);}
    buf_pushc(b,'{');if(nk==0)return buf_pushc(b,'}');
    const char **keys=(const char**)malloc(sizeof(const char*)*nk);
    char **nks=(char**)malloc(sizeof(char*)*nk);
    if(!keys||!nks){if(keys)free(keys);if(nks)free(nks);buf_free(b);luaL_error(L,"superstring: out of memory");return 0;}
    memset(nks,0,sizeof(char*)*nk);int ns=0;
    lua_pushnil(L);while(lua_next(L,idx)!=0){int kt=lua_type(L,-2);if(kt==LUA_TSTRING){keys[ns]=lua_tostring(L,-2);nks[ns]=NULL;ns++;}else if(kt==LUA_TNUMBER){char tmp[64];snprintf(tmp,sizeof(tmp),"%g",(double)lua_tonumber(L,-2));nks[ns]=strdup(tmp);keys[ns]=nks[ns];ns++;}lua_pop(L,1);}
    if(sort_keys)for(int a=0;a<ns-1;a++)for(int bb=a+1;bb<ns;bb++)if(strcmp(keys[a],keys[bb])>0){const char*tk=keys[a];keys[a]=keys[bb];keys[bb]=tk;char*tn=nks[a];nks[a]=nks[bb];nks[bb]=tn;}
    int first=1;
    for(int ki=0;ki<ns;ki++){
        lua_pushstring(L,keys[ki]);lua_gettable(L,idx);
        if(lua_isnil(L,-1)){lua_pop(L,1);continue;}
        if(!first&&!buf_pushc(b,','))goto cleanup;
        if(pretty){buf_pushc(b,'\n');buf_pushs(b,pi);}
        ss_enc_str(b,keys[ki],strlen(keys[ki]));
        pretty?buf_pushs(b,": "):buf_pushc(b,':');
        ss_enc_val(L,b,lua_gettop(L),depth+1,pretty,indent,sort_keys);
        lua_pop(L,1);first=0;
    }
    if(pretty&&!first){buf_pushc(b,'\n');buf_pushs(b,po);}
    buf_pushc(b,'}');
    for(int i=0;i<ns;i++)if(nks[i])free(nks[i]);free(keys);free(nks);return 1;
cleanup:
    for(int i=0;i<ns;i++)if(nks[i])free(nks[i]);free(keys);free(nks);return 0;
}

static int ss_enc_val(lua_State *L, Buffer *b, int idx, int depth, int pretty, int indent, int sort_keys){
    int t=lua_type(L,idx);
    switch(t){
        case LUA_TNIL:return buf_pushs(b,"null");
        case LUA_TBOOLEAN:{int v=lua_toboolean(L,idx);return buf_pushs(b,v?"yes":"no");}
        case LUA_TNUMBER:{
#if LUA_VERSION_NUM>=503
            if(cj_isinteger(L,idx)){char tmp[32];snprintf(tmp,sizeof(tmp),"%lld",(long long)lua_tointeger(L,idx));return buf_pushs(b,tmp);}
#endif
            double n=(double)lua_tonumber(L,idx);
            if(isnan(n)||isinf(n))return buf_pushs(b,"null");
            if(n==(long long)n&&fabs(n)<1e15){char tmp[32];snprintf(tmp,sizeof(tmp),"%lld",(long long)n);return buf_pushs(b,tmp);}
            char tmp[64];snprintf(tmp,sizeof(tmp),"%.17g",n);return buf_pushs(b,tmp);
        }
        case LUA_TSTRING:{size_t n;const char*s=lua_tolstring(L,idx,&n);return ss_enc_str(b,s,n);}
        case LUA_TTABLE:{lua_pushvalue(L,idx);return ss_enc_table(L,b,lua_gettop(L),depth,pretty,indent,sort_keys);}
        default:buf_free(b);luaL_error(L,"superstring.encode: unsupported type: %s",lua_typename(L,t));return 0;
    }
}

static int l_ss_encode(lua_State *L){
    luaL_checkany(L,1);int pretty=0,indent=2,sort_keys=0;
    if(lua_gettop(L)>=2&&lua_istable(L,2)){
        lua_getfield(L,2,"pretty");if(!lua_isnil(L,-1))pretty=lua_toboolean(L,-1);lua_pop(L,1);
        lua_getfield(L,2,"indent");if(lua_isnumber(L,-1))indent=(int)lua_tonumber(L,-1);lua_pop(L,1);
        lua_getfield(L,2,"sort_keys");if(!lua_isnil(L,-1))sort_keys=lua_toboolean(L,-1);lua_pop(L,1);
    }
    Buffer b;if(!buf_init(&b))luaL_error(L,"superstring.encode: out of memory");
    ss_enc_val(L,&b,1,0,pretty,indent,sort_keys);
    lua_pushlstring(L,b.data,b.len);buf_free(&b);return 1;
}

static int l_ss_decode(lua_State *L){
    size_t len;const char *s=luaL_checklstring(L,1,&len);
    Parser p;p.s=s;p.len=len;p.pos=0;p.superstring=1;p.strict=0;
    if(lua_gettop(L)>=2&&lua_istable(L,2)){lua_getfield(L,2,"strict");if(!lua_isnil(L,-1))p.strict=lua_toboolean(L,-1);lua_pop(L,1);}
    parse_val(L,&p,0);skip_ws(&p);
    if(p.pos<p.len)luaL_error(L,"superstring.decode: trailing garbage at %d",(int)p.pos);
    return 1;
}

static const luaL_Reg ss_lib[]={{"encode",l_ss_encode},{"decode",l_ss_decode},{NULL,NULL}};

#if LUA_VERSION_NUM>=502
int luaopen_superstring(lua_State *L){luaL_newlib(L,ss_lib);return 1;}
#else
int luaopen_superstring(lua_State *L){luaL_register(L,"superstring",ss_lib);return 1;}
#endif
