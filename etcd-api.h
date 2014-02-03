/*
 * Copyright (c) 2013, Red Hat
 * All rights reserved.

 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:

 * Redistributions of source code must retain the above copyright notice, this
 * list of conditions and the following disclaimer.  Redistributions in binary
 * form must reproduce the above copyright notice, this list of conditions and
 * the following disclaimer in the documentation and/or other materials
 * provided with the distribution.

 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

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
 * etcd_open_str
 *
 * Same as etcd_open, except that the servers are specified as a list of
 * host:port strings, separated by comma/semicolon or whitespace.
 */
etcd_session    etcd_open_str   (char *server_names);


/*
 * etcd_close
 *
 * Terminate a session, closing connections and freeing memory (or any other
 * resources) associated with it.
 */
void            etcd_close      (etcd_session session);


/*
 * etcd_close
 *
 * Same as etcd_close, but also free the server list as etcd_open_str would
 * have allocated it.
 */
void            etcd_close_str  (etcd_session session);


/*
 * etcd_get
 *
 * Fetch a key from one of the servers in a session.  The return value is a
 * newly allocated string, which must be freed by the caller.
 *
 *      key
 *      The etcd key (path) to fetch.
 */
char *          etcd_get (etcd_session session, char *key);


/*
 * etcd_watch
 * Watch the set of keys matching a prefix.
 *
 *      pfx
 *      The etcd key prefix (like a path) to watch.
 *
 *      keyp
 *      Space for a pointer to the key that was added/modified/deleted.
 *
 *      valuep
 *      Space for a pointer to the value if a key was added/modified.  A delete
 *      is signified by this being set to NULL.
 *
 *      index_in
 *      Pointer to an index to be used for *issuing* the watch request, or
 *      NULL for a watch without an index.
 *
 *      index_out
 *      Pointer to space for an index *returned* by etcd, or NULL to mean don't
 *      bother.
 *
 * In normal usage, index_in will be NULL and index_out will be set to receive
 * the index for the first watch.  Subsequently, index_in will be set to
 * provide the previous index (plus one) and index_out will be set to receive
 * the next.  It's entirely legitimate to point both at the same variable.
 */

etcd_result     etcd_watch (etcd_session session, char *pfx,
                            char **keyp, char **valuep,
                            int *index_in, int *index_out);


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

etcd_result     etcd_set        (etcd_session session, char *key, char *value,
                                 char *precond, unsigned int ttl);


/*
 * etcd_delete
 *
 * Delete a key from one of the servers in a session.
 *
 *      key
 *      The etcd key (path) to delete.
 */

etcd_result     etcd_delete     (etcd_session session, char *key);


/*
 * etcd_leader
 *
 * Get the identify of the current leader.
 */

char *          etcd_leader     (etcd_session session);
