#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <dbi/dbi.h>

#include "config.h"

int
main (int argc, char *argv[])
{
  dbi_conn conn;
  dbi_result result;
  int r;
  const char *cs;
  char *str1, *str2;
  char version[64];
  unsigned long long seq, n;

  if (argc < 2)
    {
      fprintf (stderr, "%s: Need a libdir as argument!\n", argv[0]);
      return 1;
    }

  dbi_initialize (argv[1]);

  conn = dbi_conn_new ("null");

  dbi_conn_get_engine_version_string (conn, version);
  assert (strcmp (version, VERSION) == 0);

  /*
   * Connection stuff
   */
  r = dbi_conn_connect (conn);
  assert (r == 0);

  r = dbi_conn_get_socket (conn);
  assert (r == 0);

  cs = dbi_conn_get_encoding (conn);
  assert (strcmp (cs, "UTF-8") == 0);

  /*
   * Quoting
   */
  str1 = strdup ("test string");
  str2 = str1;

  r = dbi_conn_quote_string (conn, &str1);
  assert (str1 != str2);
  assert (strcmp (str1, "test string") == 0);

  /*
   * DB info
   */
  result = dbi_conn_get_db_list (conn, "%");
  assert (result == NULL);

  result = dbi_conn_get_table_list (conn, "default", "%");
  assert (result == NULL);

  r = dbi_conn_select_db (conn, "default");
  assert (r == 0);

  r = dbi_conn_ping (conn);
  assert (r == 1);

  /*
   * Queries
   */
  result = dbi_conn_query (conn, "SELECT * FROM this;");
  assert (result != NULL);

  n = dbi_result_get_numrows (result);
  assert (n == 0);
  dbi_result_free (result);

  result = dbi_conn_query_null (conn,
                                (const unsigned char *)"SELECT * FROM this;",
                                (unsigned long)strlen ("SELECT * FROM this;"));
  assert (result == NULL);

  seq = dbi_conn_sequence_last (conn, "defseq");
  assert (seq == 0);

  seq = dbi_conn_sequence_next (conn, "defseq");
  assert (seq == 0);

  /*
   * Cleanup
   */
  dbi_conn_close (conn);

  dbi_shutdown ();

  return 0;
}
