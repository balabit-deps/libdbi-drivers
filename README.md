dbd-null
========

This is a very primitive database driver for [libdbi][1], that does
pretty much nothing. It does just enough to make [syslog-ng][2] work
with it without throwing errors all over the place.

That's all there is to it, nothing more, nothing less, and it will
very likely stay that way. Except when it doesn't, and the driver gets
repurposed for another use-case: simulating slow databases.

The driver now accepts a set of options, which will make the driver
sleep for the set time (if the option is unset, no sleeping will
happen) when performing the appropriate function:

* null.sleep.connect
* null.sleep.disconnect
* null.sleep.list_dbs
* null.sleep.list_tables
* null.sleep.query
* null.sleep.query_null
* null.sleep.select_db
* null.sleep.ping

 [1]: http://libdbi.sourceforge.net/
 [2]: http://www.balabit.com/network-security/syslog-ng
