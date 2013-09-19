This is a C interface to [etcd][etcd], plus a command-line tool that exercises
the API.  In brief, the supported calls are:

 * etcd_open (server-list)

 * etcd_close

 * etcd_get (key)

 * etcd_set (key, value, [optional] prev-value, [optional] ttl)

 * etcd_delete (key)

 * etcd_leader

See *etcd-api.h* for precise types and so on.  The library will automatically
try requests on a succession of servers.

The command-line utility *etcd-test* (showing its origins and primary usage so
far) can do get/set/delete/leader for you.  Note that it contains an embedded
list of servers.  Yeah, I know: ugh.  Support for specifying servers via a
config file, environment variable, and/or command line will be added soon.

This project depends on [libcurl][curl] and [YAJL][yajl] 2.x (for the tree
interface.

[etcd]: https://github.com/coreos/etcd
[curl]: http://curl.haxx.se/libcurl/
[yajl]: http://lloyd.github.io/yajl/
