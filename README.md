This is a C interface to [etcd][etcd], plus a command-line tool that exercises
the API.  In brief, the supported calls are:

 * etcd_open (server-list [as an array])

 * etcd_open_str (server-list [as a string])

 * etcd_close and etcd_close_str

 * etcd_get (key)

 * etcd_set (key, value, [optional] prev-value, [optional] ttl)

 * etcd_delete (key)

 * etcd_leader

See *etcd-api.h* for precise types and so on.  The library will automatically
try requests on a succession of servers in server-list.

The command-line utility *etcd-test* (showing its origins and primary usage so
far) can do get/set/delete/leader for you.  Servers can be specified either on
the command line (-s) or through the ETCD_SERVERS environment variableNote that it contains an embedded.

This project depends on [libcurl][curl] and [YAJL][yajl] 2.x (for the tree
interface.

[etcd]: https://github.com/coreos/etcd
[curl]: http://curl.haxx.se/libcurl/
[yajl]: http://lloyd.github.io/yajl/
