#include <assert.h>
#include <stdio.h>
#include <dbi/dbi.h>

int
main (int argc, char *argv[])
{
  dbi_conn conn;
  dbi_result result;
  int r;

  if (argc < 2)
    {
      fprintf (stderr, "%s: Need a libdir as argument!\n", argv[0]);
      return 1;
    }

  dbi_initialize (argv[1]);

  conn = dbi_conn_new ("null");

  r = dbi_conn_connect (conn);
  assert (r == 0);

  dbi_shutdown ();

  return 0;
}
