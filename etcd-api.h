/*
 * TBD
 *
 *      etcd_set: set a single value, with optional previous value as a
 *      precondition
 *
 *      etcd_watch: watch a prefix, invoking a callback when there are changes
 *      (blocks so run from its own thread)
 *
 *      etcd_elect: run a leader-election protocol using the get/set
 *      primitives, and report the result
 */

/*
 * Description of an etcd server.  For now it just includes the name and
 * port, but some day it might include other stuff like SSL certificate
 * information.
 */

typedef struct {
        char            *host;
        unsigned short  port;
} etcd_server;

typedef void *etcd_session;

/*
 * etcd_open
 *
 * Establish a session to an etcd cluster, with automatic reconnection and
 * so on.
 *
 *      server_list
 *      Array of etcd_server structures, with the last having host=NULL.  The
 *      caller is responsible for ensuring that this remains valid as long as
 *      the session exists.
 */
etcd_session    etcd_open       (etcd_server *server_list);


/*
 * etcd_close
 *
 * Terminate a session, closing connections and freeing memory (or any other
 * resources) associated with it.
 */
void            etcd_close      (etcd_session this);


/*
 * etcd_get
 *
 * Fetch a key from one of the servers in a session.
 *
 *      key
 *      The etcd key (path) to fetch.
 *
 *      buf
 *      A buffer for the received data in the key.
 *
 *      len
 *      Length of the buffer.
 */
ssize_t         etcd_get        (etcd_session this, char *key,
                                 void *buf, size_t len);
