AUTOMAKE_OPTIONS	= foreign

TESTS_ENVIRONMENT	= rootdir=${top_builddir} ${srcdir}/runtest.sh
TESTS			= test-dbd-null test-dbd-null-sleep
check_PROGRAMS		= ${TESTS}

INCLUDES		= @DBI_INCLUDE@
LIBS			= -ldbi

EXTRA_TEST		= runtest.sh
