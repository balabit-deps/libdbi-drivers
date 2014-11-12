FREETDS_OPTS = " --with-freetds --with-freetds-incdir=\${prefix}/include/ --with-freetds-libdir=\${prefix}/lib/"
MYSQL_OPTS = " --with-mysql --enable-mysql-threadsafe --with-mysql-libdir=\${prefix}/lib/ --with-mysql-incdir=\${prefix}/include/"
PGSQL_OPTS = " --with-pgsql --with-pgsql-libdir=\${prefix}/lib --with-pgsql-incdir=\${prefix}/include"
SQLITE_OPTS = " --with-sqlite3 --with-sqlite3-libdir=\${prefix}/lib --with-sqlite3-incdir=\${prefix}/include"
ORACLE_OPTS = " --with-oracle --with-oracle-libdir=\${prefix}/oracle/lib --with-oracle-incdir=\${prefix}/oracle/include"
default_opts="--with-dbi-libdir=\${prefix}/lib --with-dbi-incdir=\${prefix}/include --disable-docs " + FREETDS_OPTS + MYSQL_OPTS + PGSQL_OPTS + SQLITE_OPTS + ORACLE_OPTS
