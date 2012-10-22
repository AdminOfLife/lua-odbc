#include "utils.h"
#include "lualib.h"

const char 
  *LT_STRING  = "string",
  *LT_NUMBER  = "number",
  *LT_BOOLEAN = "boolean",
  *LT_BINARY  = "binary";


int lodbc_pass(lua_State *L){
  lua_pushboolean(L, 1);
  return 1;
}

int lodbc_push_diagnostics(lua_State *L, const SQLSMALLINT type, const SQLHANDLE handle){
    SQLCHAR State[6];
    SQLINTEGER NativeError;
    SQLSMALLINT MsgSize, i;
    SQLRETURN ret;
    char Msg[SQL_MAX_MESSAGE_LENGTH];
    luaL_Buffer b;

    luaL_buffinit(L, &b);
    i = 1;
    while (1) {
        ret = SQLGetDiagRec(type, handle, i, State, &NativeError, Msg, sizeof(Msg), &MsgSize);
        if (ret == LODBC_ODBC3_C(SQL_NO_DATA,SQL_NO_DATA_FOUND)) break;
        if(i > 1) luaL_addchar(&b, '\n');
        luaL_addlstring(&b, Msg, MsgSize);
        luaL_addchar(&b, '\n');
        luaL_addlstring(&b, State, 5);
        i++;
    }
    luaL_pushresult(&b);
    return 1;
}

int lodbc_fail(lua_State *L, const SQLSMALLINT type, const SQLHANDLE handle){
  lua_pushnil(L);
  lodbc_push_diagnostics(L, type, handle);
  return 2;
}

int lodbc_faildirect(lua_State *L, const char *err){
  lua_pushnil(L);
  lua_pushliteral(L, LODBC_PREFIX);
  lua_pushstring(L, err);
  lua_concat(L, 2);
  return 2;
}

void lodbc_stackdump( lua_State* L ) {
  int top= lua_gettop(L);
  int i;
  fprintf( stderr, "\n\tDEBUG STACK:\n" );
  if (top==0) fprintf( stderr, "\t(none)\n" );
  for( i=1; i<=top; i++ ) {
    int type= lua_type( L, i );
    fprintf( stderr, "\t[%d]= (%s) ", i, lua_typename(L,type) );

    // Print item contents here...
    //
    // Note: this requires 'tostring()' to be defined. If it is NOT,
    //       enable it for more debugging.
    //

    lua_getglobal( L, "tostring" );
    //
    // [-1]: tostring function, or nil
    if (!lua_isfunction(L,-1)) {
      fprintf( stderr, "('tostring' not available)" );
    } else {
      lua_pushvalue( L, i );
      lua_call( L, 1 /*args*/, 1 /*retvals*/ );

      // Don't trust the string contents
      //                
      fprintf( stderr, "%s", lua_tostring(L,-1) );
    }
    lua_pop(L,1);
    fprintf( stderr, "\n" );
  }
  fprintf( stderr, "\n" );
} 

int lodbc_is_fail(lua_State *L, int nresult){
  if(nresult == 0)return 0;
  return (lua_type(L, nresult) == LUA_TNIL);
}

// no result or error
int lodbc_is_unknown(lua_State *L, int nresult){
  if(nresult == 0)return 1;
  return (lua_type(L, nresult) == LUA_TNIL);
}

/*
** Check if is SQL type.
*/
int lodbc_issqltype (const SQLSMALLINT type) {
  switch (type) {
    case SQL_UNKNOWN_TYPE: case SQL_CHAR: case SQL_VARCHAR: 
    case SQL_TYPE_DATE: case SQL_TYPE_TIME: case SQL_TYPE_TIMESTAMP: 
    case SQL_DATE: case SQL_INTERVAL: case SQL_TIMESTAMP: 
    case SQL_LONGVARCHAR:
    case SQL_GUID:
    
    case SQL_NUMERIC: case SQL_DECIMAL: 
    case SQL_FLOAT: case SQL_REAL: case SQL_DOUBLE:
    case SQL_BIGINT: case SQL_TINYINT: case SQL_INTEGER: case SQL_SMALLINT:
    
    case SQL_WCHAR:
    case SQL_WVARCHAR:
    case SQL_WLONGVARCHAR:
    
    case SQL_INTERVAL_MONTH:
    case SQL_INTERVAL_YEAR:
    case SQL_INTERVAL_YEAR_TO_MONTH:
    case SQL_INTERVAL_DAY:
    case SQL_INTERVAL_HOUR:
    case SQL_INTERVAL_MINUTE:
    case SQL_INTERVAL_SECOND:
    case SQL_INTERVAL_DAY_TO_HOUR:
    case SQL_INTERVAL_DAY_TO_MINUTE:
    case SQL_INTERVAL_DAY_TO_SECOND:
    case SQL_INTERVAL_HOUR_TO_MINUTE:
    case SQL_INTERVAL_HOUR_TO_SECOND:
    case SQL_INTERVAL_MINUTE_TO_SECOND:
    
    case SQL_BINARY: case SQL_VARBINARY: case SQL_LONGVARBINARY:
    case SQL_BIT:
      return 1;
    default:
      return 0;
  }
}

/*
** Returns the name of an equivalent lua type for a SQL type.
*/
const char *lodbc_sqltypetolua (const SQLSMALLINT type) {
  switch (type) {
    case SQL_UNKNOWN_TYPE: case SQL_CHAR: case SQL_VARCHAR: 
    case SQL_TYPE_DATE: case SQL_TYPE_TIME: case SQL_TYPE_TIMESTAMP: 
    case SQL_DATE: case SQL_INTERVAL: case SQL_TIMESTAMP: 
    case SQL_LONGVARCHAR:
    case SQL_GUID:
      return LT_STRING;

    case SQL_NUMERIC: case SQL_DECIMAL: 
    case SQL_FLOAT: case SQL_REAL: case SQL_DOUBLE:
    case SQL_BIGINT: case SQL_TINYINT: case SQL_INTEGER: case SQL_SMALLINT:
      return LT_NUMBER;

    case SQL_INTERVAL_MONTH:          case SQL_INTERVAL_YEAR: 
    case SQL_INTERVAL_YEAR_TO_MONTH:  case SQL_INTERVAL_DAY:
    case SQL_INTERVAL_HOUR:           case SQL_INTERVAL_MINUTE:
    case SQL_INTERVAL_SECOND:         case SQL_INTERVAL_DAY_TO_HOUR:
    case SQL_INTERVAL_DAY_TO_MINUTE:  case SQL_INTERVAL_DAY_TO_SECOND:
    case SQL_INTERVAL_HOUR_TO_MINUTE: case SQL_INTERVAL_HOUR_TO_SECOND:
    case SQL_INTERVAL_MINUTE_TO_SECOND:  
      return LT_NUMBER; // ???

    case SQL_WCHAR: case SQL_WVARCHAR:case SQL_WLONGVARCHAR:
    case SQL_BINARY: case SQL_VARBINARY: case SQL_LONGVARBINARY:
      return LT_BINARY;	/* !!!!!! nao seria string? */

    case SQL_BIT:
      return LT_BOOLEAN;
    default:
      assert(0);
      return NULL;

  }
}


int lodbc_push_column_value(lua_State *L, SQLHSTMT hstmt, SQLUSMALLINT i, const char type){
  int top = lua_gettop(L);

  switch (type) {/* deal with data according to type */
    case 'u': { /* nUmber */
      lua_Number num;
      SQLINTEGER got;
      SQLRETURN rc = SQLGetData(hstmt, i, LODBC_C_NUMBER, &num, 0, &got);
      if (lodbc_iserror(rc)) return lodbc_fail(L, hSTMT, hstmt);
      if (got == SQL_NULL_DATA) lua_pushnil(L);
      else lua_pushnumber(L, num);
      break;
    }
    case 'o': { /* bOol */
      unsigned char b;
      SQLINTEGER got;
      SQLRETURN rc = SQLGetData(hstmt, i, SQL_C_BIT, &b, 0, &got);
      if (lodbc_iserror(rc)) return lodbc_fail(L, hSTMT, hstmt);
      if (got == SQL_NULL_DATA) lua_pushnil(L);
      else lua_pushboolean(L, b);
      break;
    }
    case 't': case 'i': {/* sTring, bInary */
      SQLSMALLINT stype = (type == 't') ? SQL_C_CHAR : SQL_C_BINARY;
      SQLINTEGER got;
      char *buffer;
      luaL_Buffer b;
      SQLRETURN rc;
      luaL_buffinit(L, &b);
      buffer = luaL_prepbuffer(&b);
      rc = SQLGetData(hstmt, i, stype, buffer, LUAL_BUFFERSIZE, &got);
      if (got == SQL_NULL_DATA){
        lua_pushnil(L);
        break;
      }
      while (rc == SQL_SUCCESS_WITH_INFO) {/* concat intermediary chunks */
        if (got >= LUAL_BUFFERSIZE || got == SQL_NO_TOTAL) {
          got = LUAL_BUFFERSIZE;
          /* get rid of null termination in string block */
          if (stype == SQL_C_CHAR) got--;
        }
        luaL_addsize(&b, got);
        buffer = luaL_prepbuffer(&b);
        rc = SQLGetData(hstmt, i, stype, buffer, LUAL_BUFFERSIZE, &got);
      }
      if (rc == SQL_SUCCESS) {/* concat last chunk */
        if (got >= LUAL_BUFFERSIZE || got == SQL_NO_TOTAL) {
          got = LUAL_BUFFERSIZE;
          /* get rid of null termination in string block */
          if (stype == SQL_C_CHAR) got--;
        }
        luaL_addsize(&b, got);
      }
      if (lodbc_iserror(rc)) return lodbc_fail(L, hSTMT, hstmt);
      /* return everything we got */
      luaL_pushresult(&b);
      break;
    }
    default:{
      // unsupported type ?
      assert(0);
    }
  }

  assert(1 == (lua_gettop(L)-top));
  return 0;
}

/*
** Retrieves data from the i_th column in the current row
** Input
**   types: index in sack of column types table
**   hstmt: statement handle
**   i: column number
** Returns:
**   0 if successfull, non-zero otherwise;
*/
int lodbc_push_column(lua_State *L, int coltypes, const SQLHSTMT hstmt, SQLUSMALLINT i) {
  const char *tname;
  char type;
  lua_rawgeti (L, coltypes, i);   /* typename of the column */
  tname = lua_tostring(L, -1);
  if (!tname) return lodbc_faildirect(L, "invalid type in table.");
  type = tname[1];
  lua_pop(L, 1); /* pops type name */
  return lodbc_push_column_value(L,hstmt,i,type);
}


typedef SQLRETURN (SQL_API*get_attr_ptr)(SQLHANDLE, SQLUSMALLINT, SQLPOINTER);
typedef SQLRETURN (SQL_API*set_attr_ptr)(SQLHANDLE, SQLUSMALLINT, SQLULEN);

static get_attr_ptr select_get_attr(SQLSMALLINT HandleType){
    switch(HandleType){
        case hDBC:  return SQLGetConnectOption;
        case hSTMT: return SQLGetStmtOption;
    }
    assert(0);
    return NULL;
}

static set_attr_ptr select_set_attr(SQLSMALLINT HandleType){
    switch(HandleType){
        case hDBC:  return SQLSetConnectOption;
        case hSTMT: return SQLSetStmtOption;
    }
    assert(0);
    return NULL;
}

typedef SQLRETURN (SQL_API*get_attr_ptr_v3)(SQLHANDLE, SQLINTEGER, SQLPOINTER, SQLINTEGER, SQLINTEGER*);
typedef SQLRETURN (SQL_API*set_attr_ptr_v3)(SQLHANDLE, SQLINTEGER, SQLPOINTER, SQLINTEGER);

static get_attr_ptr_v3 select_get_attr_v3(SQLSMALLINT HandleType){
    switch(HandleType){
        case hENV:  return SQLGetEnvAttr;
        case hDBC:  return SQLGetConnectAttr;
        case hSTMT: return SQLGetStmtAttr;
    }
    assert(0);
    return NULL;
}

static set_attr_ptr_v3 select_set_attr_v3(SQLSMALLINT HandleType){
    switch(HandleType){
        case hENV:  return SQLSetEnvAttr;
        case hDBC:  return SQLSetConnectAttr;
        case hSTMT: return SQLSetStmtAttr;
    }
    assert(0);
    return NULL;
}

int lodbc_get_uint_attr_(lua_State*L, SQLSMALLINT HandleType, SQLHANDLE Handle, SQLINTEGER optnum){
    SQLUINTEGER res;
    SQLRETURN ret;
#if (LODBC_ODBCVER >= 0x0300)
    SQLINTEGER dummy;
    ret = select_get_attr_v3(HandleType)(Handle, optnum, (SQLPOINTER)&res, sizeof(res), &dummy);
#else 
    if(HandleType == hENV) return lodbc_faildirect(L, "not supported."); 
    ret = select_get_attr   (HandleType)(Handle, optnum, (SQLPOINTER)&res);
#endif

    if(ret == LODBC_ODBC3_C(SQL_NO_DATA,SQL_NO_DATA_FOUND)) return 0;
    if(lodbc_iserror(ret)) return lodbc_fail(L, HandleType, Handle);
    lua_pushnumber(L,res);
    return 1;
}

int lodbc_get_str_attr_(lua_State*L, SQLSMALLINT HandleType, SQLHANDLE Handle, SQLINTEGER optnum){
#if (LODBC_ODBCVER >= 0x0300)
    SQLINTEGER got;
    char buffer[256];
#else 
    char buffer[SQL_MAX_OPTION_STRING_LENGTH+1];
#endif
    SQLRETURN ret;

#if (LODBC_ODBCVER >= 0x0300)
    ret = select_get_attr_v3(HandleType)(Handle, optnum, (SQLPOINTER)buffer, 255, &got);
#else 
    if(HandleType == hENV) return lodbc_faildirect(L, "not supported."); 
    ret = select_get_attr   (HandleType)(Handle, optnum, (SQLPOINTER)&buffer);
#endif

    if(ret == LODBC_ODBC3_C(SQL_NO_DATA,SQL_NO_DATA_FOUND)) return 0;
    if(lodbc_iserror(ret)) return lodbc_fail(L, HandleType, Handle);

#if (LODBC_ODBCVER >= 0x0300)
    if(got > 255){
        char* tmp = malloc(got+1);
        if(!tmp)
            return LODBC_ALLOCATE_ERROR(L);
        ret = select_get_attr_v3(HandleType)(Handle, optnum, (SQLPOINTER)tmp, got, &got);
        if(lodbc_iserror(ret)){
            free(tmp);
            if(ret == SQL_NO_DATA) return 0;
            return lodbc_fail(L, HandleType, Handle);
        }
        lua_pushstring(L, tmp);
        free(tmp);
    }
    else 
#endif
    lua_pushstring(L, buffer);

    return 1;
}

int lodbc_set_uint_attr_(lua_State*L, SQLSMALLINT HandleType, SQLHANDLE Handle, 
    SQLINTEGER optnum, SQLUINTEGER value)
{
    SQLRETURN ret; 
#if (LODBC_ODBCVER >= 0x0300)
    ret = select_set_attr_v3(HandleType)(Handle,optnum,(SQLPOINTER)value,SQL_IS_UINTEGER);
#else 
    if(HandleType == hENV) return lodbc_faildirect(L, "not supported."); 
    ret = select_set_attr   (HandleType)(Handle,optnum,value);
#endif

    if(lodbc_iserror(ret)) return lodbc_fail(L, HandleType, Handle);
    return lodbc_pass(L);
}

int lodbc_set_str_attr_(lua_State*L, SQLSMALLINT HandleType, SQLHANDLE Handle, 
    SQLINTEGER optnum, const char* value, size_t len)
{
    SQLRETURN ret;
#if (LODBC_ODBCVER >= 0x0300)
    ret = select_set_attr_v3(HandleType)(Handle,optnum,(SQLPOINTER)value,len);
#else 
    if(HandleType == hENV) return lodbc_faildirect(L, "not supported."); 
    ret = select_set_attr   (HandleType)(Handle,optnum,(SQLUINTEGER)value);
#endif
    if(lodbc_iserror(ret)) return lodbc_fail(L, HandleType, Handle);
    return lodbc_pass(L);
}
