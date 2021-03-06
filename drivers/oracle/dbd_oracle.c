/*
 * libdbi - database independent abstraction layer for C.
 * Copyright (C) 2001-2004, David Parker and Mark Tobenkin.
 * http://libdbi.sourceforge.net
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * dbd_oracle.c: Oracle database support
 * Copyright (C) 2003-2004, Christian M. Stamgren <christian@stamgren.com>
 * http://libdbi-drivers.sourceforge.net
 *
 */
 

#ifdef  HAVE_CONFIG_H
#include <config.h>
#endif

#define _GNU_SOURCE /* we need asprintf */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include <dbi/dbi.h>
#include <dbi/dbi-dev.h>
#include <dbi/dbd.h>


#include <oci.h>
#include "dbd_oracle.h"
#include "oracle_charsets.h"

/* declarations of optional external functions */
#ifndef HAVE_ASPRINTF
int asprintf(char **result, const char *format, ...);
#endif

static const dbi_info_t driver_info = {
	"Oracle",
	"Oracle database support (using Oracle Call Interface)",
	"Ashish Ranjan <ashishwave@yahoo.com>", 
	"http://libdbi-drivers.sourceforge.net",
	"dbd_Oracle v" VERSION,
	__DATE__
};

typedef struct _result_data
{
  OCIParam **params;
  ub2 *types;
} result_data;

static const char *custom_functions[] = {NULL}; 
static const char *reserved_words[] = ORACLE_RESERVED_WORDS;


void _translate_oracle_type(int fieldtype, ub1 scale, unsigned short *type, unsigned int *attribs);
void _get_field_info(dbi_result_t *result);
void _get_row_data(dbi_result_t *result, dbi_row_t *row, unsigned long long rowidx);
unsigned long long _oracle_query_to_longlong(dbi_conn_t *conn, const char *sql_cmd);
void _checkerr(OCIError * errhp, sword status);
static size_t oracle_escape_string(char *to, const char *from, size_t length);
void _oradate_to_stime (char *obuff, struct tm *stm);


void dbd_register_driver(const dbi_info_t **_driver_info, const char ***_custom_functions, 
			 const char ***_reserved_words) 
{
	*_driver_info = &driver_info;
	*_custom_functions = custom_functions;
	*_reserved_words = reserved_words;
}

int dbd_initialize(dbi_driver_t *driver) 
{
        _dbd_register_driver_cap(driver, "safe_dlclose", 1);

        return 0;
}

static sword connect_to_db(dbi_conn_t *conn, Oraconn *Oconn)
{
	const char *username  =  dbi_conn_get_option(conn, "username");
	const char *password  =  dbi_conn_get_option(conn, "password");
	const char *sid       =  dbi_conn_get_option(conn, "dbname");
	int ignore_tns_config =  dbi_conn_get_option_numeric(conn, "oracle_ignore_tns_config");
	sword status;

	if (!sid)
		sid = getenv("ORACLE_SID");

	if (ignore_tns_config)
	{
		const char *host     =  dbi_conn_get_option(conn, "host");
		const char *dbname   =  dbi_conn_get_option(conn, "dbname");
		const char *port     =  dbi_conn_get_option(conn, "port");

		if (!strlen(port))
			port = "1521";
		asprintf(&sid, "%s:%s/%s", host, port, dbname);
	}

	/* OCI ERROR HANDLE */
	if( (OCIHandleAlloc( (dvoid *) Oconn->env, (dvoid **) &(Oconn->err), OCI_HTYPE_ERROR, 
			     (size_t) 0, (dvoid **) 0)))
 	{
		_dbd_internal_error_handler(conn, "Connect::Unable to allocate ERROR handlers.", 0);
        goto error;
	}
 
	/* OCI SERVICE HANDLE */
	if( (OCIHandleAlloc( (dvoid *) Oconn->env, (dvoid **) &(Oconn->svc), OCI_HTYPE_SVCCTX,
			     (size_t) 0, (dvoid **) 0))) {
		_dbd_internal_error_handler(conn, "Connect::Unable to allocate SERVICE handlers.", 0);
        goto error;
	}
	if( (status = OCILogon(Oconn->env, Oconn->err, &(Oconn->svc), (CONST OraText*) username, 
			       strlen(username),(CONST OraText*) password, strlen(password), sid, strlen(sid)))) {
	  _checkerr(Oconn->err, status);
	  _dbd_internal_error_handler(conn, "Connect::Unable to login to the database.", 0);
       goto error;
	}

	conn->connection = (void *)Oconn;
	if (sid)
	{
		if (conn->current_db)
			free(conn->current_db);

		conn->current_db = strdup(sid);
	}

	if (ignore_tns_config)
		free(sid);
  
	return 0;

error:
    if (Oconn->err)
        OCIHandleFree(Oconn->err, OCI_HTYPE_ERROR);
    if (Oconn->svc)
        OCIHandleFree(Oconn->svc, OCI_HTYPE_SVCCTX);
    if (Oconn->env)
        OCIHandleFree(Oconn->env, OCI_HTYPE_ENV);
    if (ignore_tns_config)
        free(sid);
    free(Oconn);
    return -2;
}

static sword create_default_oracle_env(OCIEnv **env)
{
	return OCIEnvCreate(env,
		OCI_THREADED,
		(dvoid *)0,
		0,
		0,
		0,
		(size_t)0,
		(dvoid **)0);
}

static sword create_nls_oracle_env(OCIEnv **env, dbi_conn_t *conn)
{
	const char *sql_get_nchar_cmd = "select nls_charset_id('NCHAR_CS') from dual";
	const char *sql_get_char_cmd =  "select nls_charset_id('CHAR_CS') from dual";
	Oraconn *Oconn = calloc(1, sizeof(Oraconn));

	if (create_default_oracle_env(&(Oconn->env)) != 0)
		return -1;

	if (connect_to_db(conn, Oconn) != 0)
		return -1;

	long ncharset_id =  _oracle_query_to_longlong(conn, sql_get_nchar_cmd);
	long charset_id  =  _oracle_query_to_longlong(conn, sql_get_char_cmd);

	dbd_disconnect(conn);
	if (Oconn->env)
		OCIHandleFree(Oconn->env, OCI_HTYPE_ENV);

	return OCIEnvNlsCreate(env,
			(ub4)OCI_THREADED,
			(void *)0,
			(void *(*) ()) 0,
			(void *(*) ()) 0,
			(void(*) ()) 0,
			(size_t) 0,
			(void **)0,
			(ub2)charset_id,
			(ub2)ncharset_id);
}

int dbd_connect(dbi_conn_t *conn)
{
	const char *auto_charset_option = dbi_conn_get_option(conn, "oracle_auto_charset");
	int auto_charset = 0;
	sword status;
	Oraconn *Oconn = malloc(sizeof(Oraconn));
	memset(Oconn, 0, sizeof(Oraconn));

	if (auto_charset_option)
	{
		auto_charset = (strcmp(auto_charset_option, "true") == 0);
        }

	/* OCI Environment Allocation */
	if (auto_charset)
		status = create_nls_oracle_env(&(Oconn->env), conn);
	else
		status = create_default_oracle_env(&(Oconn->env));

	if (status)
	{
		_dbd_internal_error_handler(conn, "Connect::Unable to initialize environment", 0);
		/* who knows what memory area is pointed by Oconn->env...? */
		Oconn->env = NULL;
		/*free Oconn->env?*/
		return -2;
	}

	/* connect to db */
	status = connect_to_db(conn, Oconn);
	return status;
}

int dbd_disconnect(dbi_conn_t *conn) 
{
	Oraconn *Oconn = conn->connection;
  
	if (Oconn) {
		OCILogoff(Oconn->svc, 
			  Oconn->err); 
		OCIHandleFree((dvoid *) Oconn->svc, OCI_HTYPE_SVCCTX);
		OCIHandleFree((dvoid *) Oconn->err, OCI_HTYPE_ERROR); 
		free(conn->connection);
	}
    
	conn->connection = NULL;
	return 0;
}


int dbd_fetch_row(dbi_result_t *result, unsigned long long rowidx) 
{
	dbi_row_t *row = NULL;
  
	if (result->result_state == NOTHING_RETURNED) return -1;
	
	if (result->result_state == ROWS_RETURNED) {
		row = _dbd_row_allocate(result->numfields);
		_get_row_data(result, row, rowidx);
		_dbd_row_finalize(result, row, rowidx);
	}
  
	return 1; 
}


int dbd_free_query(dbi_result_t *result) 
{
  if (result->driver_data)
    {
      result_data *resinfo = result->driver_data;
      free(resinfo->params);
      free(resinfo->types);
      free(resinfo);
      result->driver_data = NULL;
    }
	if (result->result_handle) OCIHandleFree((dvoid *)result->result_handle, OCI_HTYPE_STMT);
	result->result_handle = NULL;
	return 0;
}


int dbd_goto_row(dbi_result_t *result, unsigned long long rowidx) 
{	
	return 1;
}


int dbd_get_socket(dbi_conn_t *conn)
{
	return 0; /* Oracle can't do that.. */
}

/*
 * Use this function with care... It might bite you.
 * There can be N-type columns in queries that have a diffrent Charset, 
 * a session might be altered to use another charset as output or
 * there might be values converted using the Oracle convert() function in the query. 
 */
const char *dbd_get_encoding(dbi_conn_t *conn){
	OraText  buf[OCI_NLS_MAXBUFSZ];
	sword ret;
	int i = 0;
	
	Oraconn *Oconn = conn->connection;

	if( (ret = OCINlsGetInfo(Oconn->env, Oconn->err, buf,
				 OCI_NLS_MAXBUFSZ, OCI_NLS_CHARACTER_SET)) != OCI_SUCCESS) {
		_checkerr(Oconn->err, ret);                
	}
	if (!buf) return NULL;
	else {
		while (*oracle_encoding_hash[i]) {
			if (!strcmp(oracle_encoding_hash[i], buf)) {
				return oracle_encoding_hash[i+1];
			}
			i += 2;
		}
	}
	return "UTF-16";
}

char *dbd_get_engine_version(dbi_conn_t *conn, char *versionstring) {
	/* todo: query db engine for version
	   "select version from v$instance" should do the trick */
	*versionstring = '\0';
	return versionstring;
}

dbi_result_t *dbd_list_dbs(dbi_conn_t *conn, const char *pattern) 
{
	return NULL; /* Oracle can't do that */
}


dbi_result_t *dbd_list_tables(dbi_conn_t *conn, const char *db, const char *pattern) 
{
	dbi_result_t *res;
	char *sql_cmd;
	
	/*
	 * We just ignore the db param, 
	 * Oracle can't read from diffrent databases at runtime.
	 */
	if (pattern == NULL) {
		asprintf(&sql_cmd, "SELECT table_name FROM user_tables"); 
		res = dbd_query(conn, sql_cmd);
		free(sql_cmd);
		return res;
	}
	else {
		asprintf(&sql_cmd, "SELECT table_name FROM user_tables WHERE table_name LIKE '%s'",pattern);
		res = dbd_query(conn, sql_cmd);
		free(sql_cmd);
		return res;
	}
}


size_t dbd_quote_string(dbi_driver_t *driver, const char *orig, char *dest) 
{
	size_t len;
	const char *escaped = "\'\"\\";
	strcpy(dest, "'");
	len = oracle_escape_string(dest+1, orig, strlen(orig));
	strcat(dest, "'");
	
	return len+2;
}


dbi_result_t *dbd_query_null(dbi_conn_t *conn, const char unsigned *statement, size_t st_length) 
{
	OCIStmt *stmt;
	OCIDefine *defnp;
	ub2 stmttype = 0;
	ub4 numfields = 0;
	dbi_result_t *result;
	ub4 numrows =  0, affectedrows = 0;
	Oraconn *Oconn = conn->connection;
	sword status;
	ub4 cache_rows = 0;
  result_data *resinfo = NULL;
	char *notused;

	unsigned int idx = 0;
        unsigned short fieldtype;
        unsigned int fieldattribs;
        OCIParam *param;
        ub2 otype;
        text *col_name;
        sb1  scale;
        ub4  col_name_len;
        char* col_name_dbi;


	OCIHandleAlloc( (dvoid *) Oconn->env, (dvoid **) &stmt,
			OCI_HTYPE_STMT, (size_t) 0, (dvoid **) 0);
	
	if( OCIStmtPrepare(stmt, Oconn->err, (char  *) statement,
			   (ub4) st_length, (ub4) OCI_NTV_SYNTAX, (ub4) OCI_DEFAULT)) {
                OCIHandleFree(stmt, OCI_HTYPE_STMT);
		return NULL;
	}
	
	OCIAttrGet(stmt, OCI_HTYPE_STMT, (dvoid *) &stmttype,
		   (ub4 *) 0, (ub4) OCI_ATTR_STMT_TYPE, Oconn->err);
	
	int commit_mode = OCI_COMMIT_ON_SUCCESS;
	if (!strcmp(dbi_conn_get_option(conn, "auto-commit"), "false"))
	  commit_mode = OCI_DEFAULT;
	status = OCIStmtExecute(Oconn->svc, stmt, Oconn->err, 
		       (ub4) (stmttype == OCI_STMT_SELECT ? 0 : 1), 
		       (ub4) 0, (CONST OCISnapshot *) NULL, (OCISnapshot *) NULL, 
		       stmttype == OCI_STMT_SELECT ? MY_OCI_STMT_SCROLLABLE_READONLY : commit_mode);

	if( status != OCI_SUCCESS) {
	  OCIHandleFree(stmt, OCI_HTYPE_STMT);
	  return NULL;
	}
       
	if( stmttype == OCI_STMT_SELECT) { 

		OCIAttrGet (stmt, OCI_HTYPE_STMT, (dvoid *) &numfields, 
			    (ub4 *) 0, (ub4) OCI_ATTR_PARAM_COUNT, Oconn->err); 

		
		/* 
		 * To find out how many rows there is in a result set we need to call 
		 * OCIStmtFetch2() with OCI_FETCH_LAST and then use OCIAttrGet()
		 * with MY_OCI_ATTR_CURRENT_POSITION, This is really not that great 
		 * because it might be very very very slow..... But It's the only way I know.
		 * It would be really great if libdbi didn't have to know how large a result set is
		 * at this early point.
		 */

		/* dummy define .. 
		 * We need to have atleast one define before fetching. Duh!!! 
		 */
		OCIDefineByPos(stmt, &defnp, Oconn->err, 1, (dvoid *) &notused,
			       (sword) sizeof(sword), SQLT_CHR, (dvoid *) 0, (ub2 *)0,
			       (ub2 *)0, OCI_DEFAULT); 

		
		status = OCIStmtFetch2(stmt, Oconn->err,
			       (ub4)1, OCI_FETCH_LAST, 0, OCI_DEFAULT);

		status = OCIAttrGet (stmt, OCI_HTYPE_STMT, (dvoid *) &numrows, 
				     (ub4 *) 0, (ub4) MY_OCI_ATTR_CURRENT_POSITION, Oconn->err); 
		_checkerr(Oconn->err, status);
		
		/* cache should be about 20% of all rows. */
		cache_rows = dbi_conn_get_option_numeric(conn, "oracle_prefetch_rows");
    if ((int)cache_rows > 0) {
			OCIAttrSet(stmt, OCI_HTYPE_STMT,
				   &cache_rows, sizeof(cache_rows), OCI_ATTR_PREFETCH_ROWS,
				   Oconn->err);
		}

		/* howto handle affectedrows? */
	}

	result = _dbd_result_create(conn, (void *)stmt, numrows , affectedrows);
	_dbd_result_set_numfields(result, numfields);
  resinfo = malloc(sizeof(result_data));
  resinfo->params = malloc(sizeof(OCIParam *) * numfields);
  resinfo->types = malloc(sizeof(ub2) * numfields);
  result->driver_data = resinfo;

	while (idx < result->numfields) {
                scale = 0;

                OCIParamGet(stmt, OCI_HTYPE_STMT, Oconn->err,(dvoid **)&param,
                            (ub4) idx+1);
                resinfo->params[idx] = param;

                OCIAttrGet(param, (ub4) OCI_DTYPE_PARAM,
                           &otype, (ub4 *) 0, (ub4) OCI_ATTR_DATA_TYPE,
                           (OCIError *) Oconn->err  );
                resinfo->types[idx] = otype;

                OCIAttrGet((dvoid*) param, (ub4) OCI_DTYPE_PARAM,
                           (dvoid**) &col_name,(ub4 *) &col_name_len, (ub4) OCI_ATTR_NAME,
                           (OCIError *) Oconn->err );

                if(otype == 2) { /* we got SQLT_NUM */
                        OCIAttrGet((dvoid*) param, (ub4) OCI_DTYPE_PARAM,
                                   (dvoid**) &scale,(ub4 *) 0, (ub4) OCI_ATTR_SCALE,
                                   (OCIError *) Oconn->err );
                }
                /*bug fixing , it is necessary to copy it to earlier as in many cases it is not giving a properly terminated string, which gives malformed col name */
                col_name_dbi = calloc(col_name_len + 1,sizeof(char) );
                strncpy( col_name_dbi, (char *) col_name,  col_name_len);

                _translate_oracle_type(otype, scale, &fieldtype, &fieldattribs);
                _dbd_result_add_field(result, idx, col_name_dbi, fieldtype, fieldattribs);

                free(col_name_dbi);
                idx++;
        }
        //end-code block1
	
	return result;
}

dbi_result_t *dbd_query(dbi_conn_t *conn, const char *statement) 
{
	return dbd_query_null(conn, statement, strlen(statement));
}


const char *dbd_select_db(dbi_conn_t *conn, const char *db) 
{
  /* todo: PostgreSQL can't do it either, but there's a workaround */
	return NULL; /* Oracle can't do that .... */
}

int dbd_geterror(dbi_conn_t *conn, int *errno, char **errstr) 
{
	char errbuf[1024];
	int  errcode = 0;
	Oraconn *Oconn = conn->connection;
	*errno = 0;

	if (!conn->connection) {
		*errstr = strdup("Unable to connect to database.");
		return 2;
	} else {
		OCIErrorGet((dvoid *)Oconn->err, (ub4) 1, (text *) NULL, &errcode, errbuf, 
			    (ub4) sizeof(errbuf), OCI_HTYPE_ERROR);
		*errstr = strdup(errbuf);
		*errno = errcode;
  }
	
	return 3;
}

unsigned long long dbd_get_seq_last(dbi_conn_t *conn, const char *sequence) 
{  
	unsigned long long retval = 0;
	char *sql_cmd = NULL;
	
	if( ! sequence) return 0; 
	
	asprintf(&sql_cmd, "SELECT %s.currval FROM dual", sequence);
	
	retval =  _oracle_query_to_longlong(conn, sql_cmd);
	if(sql_cmd) free(sql_cmd);

	return retval;
}

unsigned long long dbd_get_seq_next(dbi_conn_t *conn, const char *sequence) 
{	
	unsigned long long retval = 0;
	char *sql_cmd = NULL;
   
    if( ! sequence) return 0; 
   
	asprintf(&sql_cmd, "SELECT %s.nextval FROM dual", sequence);

	retval =  _oracle_query_to_longlong(conn, sql_cmd);
	if(sql_cmd) free(sql_cmd);

	return retval;
}


int dbd_ping(dbi_conn_t *conn) 
{
	unsigned long long retval = 0;
	char *sql_cmd = NULL;
/* 	Oraconn *Oconn = conn->connection; */

	asprintf(&sql_cmd, "SELECT 1 from dual");
	
	retval =  _oracle_query_to_longlong(conn, sql_cmd);
	if(sql_cmd) free(sql_cmd);

	if(!retval) { /* We need to reconnect! */
		dbd_disconnect(conn);
		retval = dbd_connect(conn);

		return (retval ? 0 : retval); 

	}
	return retval;
}

/* CORE ORACLE DATA FETCHING STUFF */
void _translate_oracle_type(int fieldtype, ub1 scale, unsigned short *type, unsigned int *attribs) 
{
	unsigned int _type = 0;
	unsigned int _attribs = 0;

	switch (fieldtype) {

	case SQLT_INT:
	case SQLT_LNG:
		_type = DBI_TYPE_INTEGER;
		_attribs |= DBI_INTEGER_SIZE8;
		break;
	case SQLT_FLT:
		_type = DBI_TYPE_DECIMAL;
		_attribs |= DBI_DECIMAL_SIZE8;
		break;
	case SQLT_NUM:
		/*
		 * We use size8 becouse we don't know if it
		 * can be smaller :( 
		 */
		if(scale > 0) {
			_type = DBI_TYPE_DECIMAL;
			_attribs |= DBI_DECIMAL_SIZE8;
		} else {
			_type = DBI_TYPE_INTEGER;
			_attribs |= DBI_INTEGER_SIZE8;
		}
		break;
		
	case SQLT_BIN:
	case SQLT_LBI:
		_type = DBI_TYPE_BINARY;
		break;
	  
	case SQLT_DAT:
  case SQLT_TIMESTAMP:
  case SQLT_DATE:
  case SQLT_TIMESTAMP_TZ:
  case SQLT_TIMESTAMP_LTZ:
		 _type = DBI_TYPE_DATETIME;
    break;
	case SQLT_AFC:
	case SQLT_STR:
	case SQLT_CHR:

	case SQLT_VNU:
	case SQLT_VCS:
	default:
		_type = DBI_TYPE_STRING;
		break;
	}
	
	*type = _type;
	*attribs = _attribs;
}
 
void _get_field_info(dbi_result_t *result) 
{
	unsigned int idx = 0;
	unsigned short fieldtype;
	unsigned int fieldattribs;
	OCIParam *param = NULL;
	ub4 otype;
	text *col_name;
	sb1  scale;
	ub4  col_name_len;

	char* col_name_dbi;

	Oraconn *Oconn = (Oraconn *)result->conn->connection;
        OCIStmt *stmt  = (OCIStmt *) result->result_handle;
	
	while (idx < result->numfields) {
		scale = 0;
		
		OCIParamGet((dvoid *)stmt, OCI_HTYPE_STMT, Oconn->err, (dvoid **)&param,
			    (ub4) idx+1);
		
		OCIAttrGet((dvoid*) param, (ub4) OCI_DTYPE_PARAM,
			   (dvoid*) &otype,(ub4 *) 0, (ub4) OCI_ATTR_DATA_TYPE,
			   (OCIError *) Oconn->err  );
	 
		OCIAttrGet((dvoid*) param, (ub4) OCI_DTYPE_PARAM,
			   (dvoid**) &col_name,(ub4 *) &col_name_len, (ub4) OCI_ATTR_NAME,
			   (OCIError *) Oconn->err );
	
		if(otype == 2) { /* we got SQLT_NUM */
			OCIAttrGet((dvoid*) param, (ub4) OCI_DTYPE_PARAM,
				   (dvoid**) &scale,(ub4 *) 0, (ub4) OCI_ATTR_SCALE,
				   (OCIError *) Oconn->err );
		}
	  	/*bug fixing , it is necessary to copy it to earlier as in many cases it is not giving a properly terminated string, which gives malformed col name */
		col_name_dbi = calloc(col_name_len + 1,sizeof(char) );
		strncpy( col_name_dbi, (char *) col_name,  col_name_len);

		_translate_oracle_type(otype, scale, &fieldtype, &fieldattribs);
		_dbd_result_add_field(result, idx, col_name_dbi, fieldtype, fieldattribs);

		free(col_name_dbi);
		idx++;
	}
}

void _get_row_data(dbi_result_t *result, dbi_row_t *row, unsigned long long rowidx) 
{
	OCIStmt *stmt = result->result_handle;
	OCIDefine *defnp = (OCIDefine *) 0;
	OCIParam *param;
	Oraconn *Oconn = result->conn->connection; 
	unsigned int curfield = 0;
	ub2 length = 0;
	size_t slen;
	unsigned int sizeattrib;
	dbi_data_t *data;
  ub4 rc;
  ub2 type;
	char *ptr, *cols[result->numfields];
  ub4 col_types[result->numfields];
  sb2 year;
  ub1 mon,day,hour,min,sec;
  ub4 fsec;
	sword status;
  struct tm *tmt;
  result_data *resinfo = result->driver_data;

	/* 
	 * Prefetch all cols as char *'s 
	 * This might not be all that good  ... lets revisit
	 * this when some Oracle guru starts sending in patches.
	 */
	while(curfield < result->numfields) {
		length = 0;

		OCIAttrGet((dvoid*) resinfo->params[curfield], (ub4) OCI_DTYPE_PARAM,
			   (dvoid*) &length,(ub4 *) 0, (ub4) OCI_ATTR_DATA_SIZE,
			   (OCIError *) Oconn->err  );
    type = resinfo->types[curfield];
		if (result->field_types[curfield] == DBI_TYPE_DATETIME)
      {
        if (type != SQLT_DAT)
          {
            OCIDateTime *stamp;
            switch (type)
              {
                case  SQLT_TIMESTAMP:
                  {
                    col_types[curfield] = OCI_DTYPE_TIMESTAMP;
                    break;
                  }
                case SQLT_DATE:
                  {
                    col_types[curfield] = OCI_DTYPE_DATE;
                    break;
                  }
                case SQLT_DAT:
                  {
                    col_types[curfield] = OCI_DTYPE_DATE;
                    break;
                  }
                case SQLT_TIMESTAMP_TZ:
                  {
                    col_types[curfield] = OCI_DTYPE_TIMESTAMP_TZ;
                    break;
                  }
                case SQLT_TIMESTAMP_LTZ:
                  {
                    col_types[curfield] = OCI_DTYPE_TIMESTAMP_LTZ;
                    break;
                  }
              }
            OCIDescriptorAlloc(Oconn->env, (dvoid **)&stamp, col_types[curfield], 0, NULL);
            OCIDefineByPos(stmt, &defnp, Oconn->err, curfield+1, &stamp, length+1, (ub2) type, (dvoid *) 0, (ub2 *)0, (ub2 *)0, OCI_DEFAULT);
            cols[curfield] = (char *)stamp;
          }
        else
          {
		        cols[curfield] = (char *)malloc(length+1);
            OCIDefineByPos(stmt, &defnp, Oconn->err, curfield+1, cols[curfield], (sword) length+1, SQLT_DAT, (dvoid *) 0, (ub2 *)0, (ub2 *)0, OCI_DEFAULT);
            col_types[curfield] = 0;
          }
      }
		else
      {
		    cols[curfield] = (char *)malloc(length+1);
        OCIDefineByPos(stmt, &defnp, Oconn->err, curfield+1, cols[curfield], (sword) length+1, SQLT_STR, (dvoid *) 0, (ub2 *)0, (ub2 *)0, OCI_DEFAULT);
      }

		if(length < 0 ) _set_field_flag( row, curfield, DBI_VALUE_NULL, 1);

		switch (result->field_types[curfield]) {
		case DBI_TYPE_BINARY:
		case DBI_TYPE_STRING:
			row->field_sizes[curfield] = length;
			break;
		
		default:
			row->field_sizes[curfield] = 0;
			break;
		}
		curfield++;

	}

	status = OCIStmtFetch2(stmt, Oconn->err, 1, OCI_FETCH_ABSOLUTE, rowidx+1, OCI_DEFAULT);

	curfield = 0;
	while (curfield < result->numfields) {

		data = &row->field_values[curfield];

		switch (result->field_types[curfield]) {
		case DBI_TYPE_INTEGER:
			sizeattrib = _isolate_attrib(result->field_attribs[curfield], DBI_INTEGER_SIZE1, DBI_INTEGER_SIZE8);
			switch (sizeattrib) {
			case DBI_INTEGER_SIZE1:
			case DBI_INTEGER_SIZE2:
			case DBI_INTEGER_SIZE3:
			case DBI_INTEGER_SIZE4:
			case DBI_INTEGER_SIZE8:
			default:
				data->d_longlong = (long long) atoll(cols[curfield]);  
				break;
			}

			break;
		case DBI_TYPE_DECIMAL:
			sizeattrib = _isolate_attrib(result->field_attribs[curfield], DBI_DECIMAL_SIZE4, DBI_DECIMAL_SIZE8);
			switch (sizeattrib) {
			case DBI_DECIMAL_SIZE4:
			case DBI_DECIMAL_SIZE8:
			default:			
				data->d_double = (double) strtod(cols[curfield], NULL);
				//fprintf(stderr, "Double: %s:%f\n", cols[curfield], data->d_double);
				break;
			}

			break;

		case DBI_TYPE_STRING:
			
      slen = row->field_sizes[curfield];
			
			data->d_string = malloc(row->field_sizes[curfield]+1);
			memcpy(data->d_string, cols[curfield],row->field_sizes[curfield]);
			data->d_string[slen] = '\0';
			if (dbi_conn_get_option_numeric(result->conn, "oracle_chop_blanks") == 1) {
				ptr = data->d_string;

				if(ptr != NULL && *ptr != '\0') 
					while(slen && ptr[slen - 1] == ' ')
						--slen;
				
				ptr[slen] = '\0'; /* Chop blanks */
				row->field_sizes[curfield] = slen; /* alter field length */
			}
			break;
		case DBI_TYPE_BINARY:
			data->d_string = malloc(row->field_sizes[curfield]);
			memcpy(data->d_string, cols[curfield],row->field_sizes[curfield]);
			break;
		 case DBI_TYPE_DATETIME:
      if (col_types[curfield]!=0)
        {
          tmt=&(data->d_stime);
          memset(tmt,0,sizeof(struct tm));
          rc = OCIDateTimeGetDate(Oconn->env, Oconn->err, (OCIDateTime *)cols[curfield], &year, &mon, &day);
          if (rc == OCI_SUCCESS)
            {
              tmt->tm_year = (int )(year - 1900);
              tmt->tm_mon = (int )(mon - 1);
              tmt->tm_mday = (int )day;
            }
          rc = OCIDateTimeGetTime(Oconn->env, Oconn->err, (OCIDateTime *)cols[curfield], &hour, &min, &sec, &fsec);
          if (rc == OCI_SUCCESS)
            {
              tmt->tm_hour = (int )hour;
              tmt->tm_min = (int )min;
              tmt->tm_sec = (int )sec;
            }
          OCIDescriptorFree(cols[curfield],col_types[curfield]);
          cols[curfield] = NULL;
        }
      else
        {
          _oradate_to_stime (cols[curfield], &(data->d_stime));
        }
			break;
		}
		if (cols[curfield]) free(cols[curfield]);
		curfield++;
	}		
}

unsigned long long _oracle_query_to_longlong(dbi_conn_t *conn, const char *query)
{
	OCIStmt *stmt = NULL;
	sword val = 0;
	OCIDefine *defnp = NULL;
	
	Oraconn *Oconn = conn->connection;
	
	OCIHandleAlloc( (dvoid *) Oconn->env, (dvoid **) &stmt,
			OCI_HTYPE_STMT, (size_t) 0, (dvoid **) 0);
	OCIStmtPrepare(stmt, Oconn->err, (char  *) query,
		       (ub4) strlen(query), (ub4) OCI_NTV_SYNTAX, (ub4) OCI_DEFAULT);
		
	OCIDefineByPos(stmt, &defnp, Oconn->err, 1, (dvoid *) &val,
		       (sword) sizeof(sword), SQLT_INT, (dvoid *) 0, (ub2 *)0,
		       (ub2 *)0, OCI_DEFAULT);
  
	OCIStmtExecute(Oconn->svc, stmt, Oconn->err, 
		       (ub4) 1, (ub4) 0, (CONST OCISnapshot *) NULL, (OCISnapshot *) NULL, 
		       OCI_DEFAULT); 
  
	OCIStmtFetch(stmt, Oconn->err, (ub4) 1, (ub4) OCI_FETCH_NEXT,
		     (ub4) OCI_DEFAULT);
  
	(void) OCIHandleFree((dvoid *) stmt, OCI_HTYPE_STMT);
	
	return (unsigned long long) (val ? val : 0);
}


void _checkerr(OCIError * errhp, sword status)
{
	text errbuf[512];
/* 	ub4 buflen; */
	ub4 errcode;

	switch (status)
	{
	case OCI_SUCCESS:
		break;
	case OCI_SUCCESS_WITH_INFO:
		(void) fprintf(stderr,"Error - OCI_SUCCESS_WITH_INFO\n");
		break;
	case OCI_NEED_DATA:
		(void) fprintf(stderr, "Error - OCI_NEED_DATA\n");
		break;
	case OCI_NO_DATA:
		(void) fprintf(stderr,"Error - OCI_NODATA\n");
		break;
	case OCI_ERROR:
		(void) OCIErrorGet (errhp, (ub4) 1, (text *) NULL, &errcode,
				    errbuf, (ub4) sizeof(errbuf), OCI_HTYPE_ERROR);

		(void) fprintf(stderr,"Error - %s Code: %d\n", errbuf, errcode);
		break;
	case OCI_INVALID_HANDLE:
		(void) fprintf(stderr,"Error - OCI_INVALID_HANDLE\n");
		break;
	case OCI_STILL_EXECUTING:
		(void) fprintf(stderr,"Error - OCI_STILL_EXECUTE\n");
		break;
	default:
		break;
	}
}

const char *dbd_encoding_from_iana(const char * iana_encoding )
{
	return iana_encoding;
}

const char *dbd_encoding_to_iana(const char * iana_encoding )
{
	return iana_encoding;
}

/* taken from sqlite3 driver */
size_t dbd_quote_binary (dbi_conn_t *conn, const unsigned char *orig, size_t from_length, unsigned char **ptr_dest ) {
  unsigned char *temp;
  size_t len;

  if ((temp = (unsigned char*)malloc(from_length*2)) == NULL) {
    return 0;
  }

  strcpy(temp, "\'");
  if (from_length) {
    len = _dbd_encode_binary(orig, from_length, temp+1);
  }
  else {
    len = 0;
  }
  strcat(temp, "'");

  *ptr_dest = temp;

  return len+2;

}

size_t dbd_conn_quote_string (dbi_conn_t *conn, const char *orig, char *dest ) {
	return dbd_quote_string(conn->driver, orig, dest);
}




/* This function is stolen from MySQL,Sqlite3. The quoting was changed to the
 SQL standard, i.e. single and double quotes are escaped by doubling,
 not by a backslash. Newlines and carriage returns are left alone */
static size_t oracle_escape_string(char *to, const char *from, size_t length)
{
  const char *to_start=to;
  const char *end;

  for (end=from+length; from != end ; from++)
    {
      switch (*from) {
      case 0:                           /* Must be escaped for 'mysql' */
        *to++= '\\';
        *to++= '0';
        break;
      case '\'':
        *to++= '\''; /* double single quote */
        *to++= '\'';
        break;
      case '\032':                      /* This gives problems on Win32 */
        *to++= '\\';
        *to++= 'Z';
        break;
      default:
        *to++= *from;
      }
    }
  *to=0;
  return (size_t) (to-to_start);
}



void _oradate_to_stime (char *obuff, struct tm *stm)
{
  struct  tm *tmt=stm;
/*  char    stime[101], *cp = NULL; */
  time_t  loct = 0L;

/*    memset(stime, 0, sizeof(stime)); */
   
/*    sprintf(stime, "%04d%02d%02d%02d%02d%02d",  */
/*    //             |         YYYY           |                   */
/*                 (obuff[0]-100)*100 + (obuff[1]-100), */
/*    //                 | month |   day   | */
/*                       obuff[2], obuff[3],  */
/*    //            |  HH     |   MM      |   SS       | */
/* 		 obuff[4]-1, obuff[5]-1, obuff[6]-1);    */

/*    cp = strptime(stime, "%Y%m%d%H%M%S", &tmt); */

  memset(tmt, 0, sizeof(struct tm));
  tmt->tm_sec = obuff[6]-1;
  tmt->tm_min = obuff[5]-1;
  tmt->tm_hour = obuff[4]-1;
  tmt->tm_mday = obuff[3];
  tmt->tm_mon = obuff[2]-1;
  tmt->tm_year = (obuff[0]-100)*100 + (obuff[1]-100);

  tmt->tm_year -= 1900;
}
