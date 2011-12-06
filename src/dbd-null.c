/* dbd-null - dummy database driver for libdbi.
 * Copyright (C) 2011 Gergely Nagy <algernon@balabit.hu>
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301 USA
 */

#define GNU_SOURCE 1

#include <dbi/dbi.h>
#include <dbi/dbi-dev.h>
#include <dbi/dbd.h>

#include <string.h>
#include <unistd.h>

#include "config.h"

static const dbi_info_t driver_info = {
        "null",
        "dummy database driver",
        "Gergely Nagy <algernon@balabit.hu>",
        "none",
        "dbd_null v" VERSION,
        __DATE__
};

static const char *custom_functions[] = { NULL };
static const char *reserved_words[] = { NULL };

static inline void
_dbd_null_sleep (dbi_conn_t *conn, const char *opt_name)
{
  int n;

  n = dbi_conn_get_option_numeric (conn, opt_name);
  if (n > 0)
    sleep (n);
}

void
dbd_register_driver (const dbi_info_t **_driver_info, const char ***_custom_functions,
		     const char ***_reserved_words)
{
        *_driver_info = &driver_info;
        *_custom_functions = custom_functions;
        *_reserved_words = reserved_words;
}

int
dbd_initialize (dbi_driver_t *driver)
{
        _dbd_register_driver_cap(driver, "safe_dlclose", 1);
        return 0;
}

int
dbd_connect (dbi_conn_t *conn)
{
  _dbd_null_sleep (conn, "null.sleep.connect");
  return 0;
}

int
dbd_disconnect (dbi_conn_t *conn)
{
  _dbd_null_sleep (conn, "null.sleep.disconnect");
  return 0;
}

int
dbd_fetch_row (dbi_result_t *result, unsigned long long rowidx)
{
  return 0;
}

int
dbd_free_query (dbi_result_t *result)
{
  return 0;
}

int
dbd_goto_row (dbi_result_t *result, unsigned long long rowidx)
{
  result->currowidx = rowidx;
  return 1;
}

int
dbd_get_socket (dbi_conn_t *conn)
{
  return 0;
}

const char *
dbd_get_encoding (dbi_conn_t *conn)
{
  return "UTF-8";
}

const char *
dbd_encoding_to_iana (const char *db_encoding)
{
  return db_encoding;
}

const char *
dbd_encoding_from_iana (const char *iana_encoding)
{
  return iana_encoding;
}

char *
dbd_get_engine_version (dbi_conn_t *conn, char *versionstring)
{
  strncpy (versionstring, VERSION, VERSIONSTRING_LENGTH + 1);
  return versionstring;
}

dbi_result_t *
dbd_list_dbs (dbi_conn_t *conn, const char *pattern)
{
  _dbd_null_sleep (conn, "null.sleep.list_dbs");
  return NULL;
}

dbi_result_t *
dbd_list_tables (dbi_conn_t *conn, const char *db, const char *pattern)
{
  _dbd_null_sleep (conn, "null.sleep.list_tables");
  return NULL;
}

size_t
dbd_quote_string (dbi_driver_t *driver, const char *orig, char *dest)
{
  strcpy (dest, orig);
  return strlen (dest);
}

size_t
dbd_conn_quote_string (dbi_conn_t *conn, const char *orig, char *dest)
{
  return dbd_quote_string (NULL, orig, dest);
}

size_t
dbd_quote_binary (dbi_conn_t *conn, const unsigned char *orig, size_t from_length,
		  unsigned char **ptr_dest)
{
  unsigned char *tmp;

  tmp = malloc (from_length);
  memcpy (tmp, orig, from_length);
  *ptr_dest = tmp;

  return from_length;
}

dbi_result_t *
dbd_query (dbi_conn_t *conn, const char *statement)
{
  dbi_result_t *res;

  res = _dbd_result_create (conn, NULL, 0, 0);
  _dbd_result_set_numfields (res, 0);

  _dbd_null_sleep (conn, "null.sleep.query");
  return res;
}

dbi_result_t *
dbd_query_null (dbi_conn_t *conn, const unsigned char *statement, size_t st_length)
{
  _dbd_null_sleep (conn, "null.sleep.query_null");
  return NULL;
}

const char *
dbd_select_db (dbi_conn_t *conn, const char *db)
{
  _dbd_null_sleep (conn, "null.sleep.select_db");
  return db;
}

int
dbd_geterror (dbi_conn_t *conn, int *errno, char **errstr)
{
  *errno = 0;
  *errstr = NULL;
  return 3;
}

unsigned long long
dbd_get_seq_last (dbi_conn_t *conn, const char *sequence)
{
  return 0;
}
    
unsigned long long
dbd_get_seq_next (dbi_conn_t *conn, const char *sequence)
{
  _dbd_internal_error_handler (conn, NULL, DBI_ERROR_UNSUPPORTED);
  return 0;
}

int
dbd_ping (dbi_conn_t *conn)
{
  _dbd_null_sleep (conn, "null.sleep.ping");
  return 1;
}
