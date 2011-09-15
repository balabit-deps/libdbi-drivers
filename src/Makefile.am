AUTOMAKE_OPTIONS	= foreign

null_ltlibs		= libdbdnull.la
null_ldflags		= -module -avoid-version
null_sources		= dbd-null.c

INCLUDES		= @DBI_INCLUDE@

libdir			= ${driverdir}
lib_LTLIBRARIES		= ${null_ltlibs}
libdbdnull_la_LDFLAGS	= ${null_ldflags} -ldbi
libdbdnull_la_SOURCES	= ${null_sources}
