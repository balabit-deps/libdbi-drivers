#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <dbi/dbi.h>

#define assert_time(func,interval)              \
  start = time (NULL);                          \
  func;                                         \
  printf (".");                                 \
  fflush (NULL);                                \
  end = time (NULL);                            \
  assert (end - start >= interval)

int
main (int argc, char *argv[])
{
  dbi_conn conn;
  dbi_result result;
  time_t start, end;

  if (argc < 2)
    {
      fprintf (stderr, "%s: Need a libdir as argument!\n", argv[0]);
      return 1;
    }

  dbi_initialize (argv[1]);

  conn = dbi_conn_new ("null");

  /*
   * Connection stuff
   */
  dbi_conn_set_option_numeric (conn, "null.sleep.connect", 5);
  assert_time (dbi_conn_connect (conn), 3);

  /*
   * DB info
   */
  dbi_conn_set_option_numeric (conn, "null.sleep.list_dbs", 5);
  assert_time (dbi_conn_get_db_list (conn, "%"), 3);

  dbi_conn_set_option_numeric (conn, "null.sleep.list_tables", 5);
  assert_time (dbi_conn_get_table_list (conn, "default", "%"), 3);

  dbi_conn_set_option_numeric (conn, "null.sleep.select_db", 5);
  assert_time (dbi_conn_select_db (conn, "default"), 3);

  dbi_conn_set_option_numeric (conn, "null.sleep.ping", 5);
  assert_time (dbi_conn_ping (conn), 3);

  /*
   * Queries
   */
  dbi_conn_set_option_numeric (conn, "null.sleep.query", 5);
  assert_time (result = dbi_conn_query (conn, "SELECT * FROM this;"), 3);
  dbi_result_free (result);

  dbi_conn_set_option_numeric (conn, "null.sleep.query_null", 5);
  assert_time (dbi_conn_query_null (conn,
                                    (const unsigned char *)"SELECT * FROM this;",
                                    (const unsigned long)strlen ("SELECT * FROM this;")),
               3);

  /*
   * Cleanup
   */
  dbi_conn_close (conn);

  dbi_shutdown ();

  printf ("\r");

  return 0;
}
