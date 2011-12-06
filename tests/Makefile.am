AUTOMAKE_OPTIONS	= foreign

TESTS_ENVIRONMENT	= rootdir=${top_builddir} ${srcdir}/runtest.sh
TESTS			= test-dbd-null
check_PROGRAMS		= ${TESTS}

INCLUDES		= @DBI_INCLUDE@
LIBS			= -ldbi

EXTRA_TEST		= runtest.sh

test_dbd_null_SOURCES	= test-dbd-null.c
