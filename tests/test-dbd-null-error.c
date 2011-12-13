#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <dbi/dbi.h>

int
main (int argc, char *argv[])
{
  dbi_conn conn;
  dbi_result result;

  if (argc < 2)
    {
      fprintf (stderr, "%s: Need a libdir as argument!\n", argv[0]);
      return 1;
    }

  dbi_initialize (argv[1]);

  conn = dbi_conn_new ("null");
  dbi_conn_connect (conn);

  /*
   * Queries
   */
  result = dbi_conn_query (conn, "COMMIT");
  assert (result != NULL);
  dbi_result_free (result);

  dbi_conn_set_option_numeric (conn, "null.error.commit", 1);
  result = dbi_conn_query (conn, "COMMIT");
  assert (result == NULL);

  dbi_conn_set_option_numeric (conn, "null.error.commit", 0);
  result = dbi_conn_query (conn, "COMMIT");
  assert (result != NULL);
  dbi_result_free (result);

  /*
   * Cleanup
   */
  dbi_conn_close (conn);

  dbi_shutdown ();

  return 0;
}
