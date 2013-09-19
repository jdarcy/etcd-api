/*
 * Description of an etcd server.  For now it just includes the name and
 * port, but some day it might include other stuff like SSL certificate
 * information.
 */

typedef enum {
        ETCD_OK = 0,
        ETCD_PROTOCOL_ERROR,
                                /* TBD: add other error categories here */
        ETCD_WTF                /* anything we can't easily categorize */
} etcd_result;

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
 * Fetch a key from one of the servers in a session.  The return value is a
 * newly allocated string, which must be freed by the caller.
 *
 *      key
 *      The etcd key (path) to fetch.
 */
char *          etcd_get (etcd_session this, char *key);


/*
 * etcd_set
 *
 * Write a key, with optional TTL and/or previous value (as a precondition).
 *
 *      key
 *      The etcd key (path) to set.
 *
 *      value
 *      New value as a null-terminated string.  Unlike etcd_get, we can derive
 *      the length ourselves instead of needing it to be passed in separately.
 *
 *      precond
 *      Required previous value as a null-terminated string, or NULL to mean
 *      an unconditional set.
 *
 *      ttl
 *      Time in seconds after which the value will automatically expire and be
 *      deleted, or zero to mean no auto-expiration.
 */

etcd_result     etcd_set        (etcd_session this, char *key, char *value,
                                 char *precond, unsigned int ttl);


/*
 * etcd_delete
 *
 * Delete a key from one of the servers in a session.
 *
 *      key
 *      The etcd key (path) to delete.
 */

etcd_result     etcd_delete     (etcd_session this, char *key);


/*
 * etcd_leader
 *
 * Get the identify of the current leader.
 */

char *          etcd_leader     (etcd_session this_as_void);
