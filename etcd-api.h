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
void            etcd_close      (etcd_session this);


/*
 * etcd_close
 *
 * Same as etcd_close, but also free the server list as etcd_open_str would
 * have allocated it.
 */
void            etcd_close_str  (etcd_session this_as_void);


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


/*
 * TBD: add etcd_watch
 *
 * This is mostly like GET, but with two extra sources of complexity.  The
 * first is that we need to parse/return a key and *maybe* a value.  We can
 * probably signal a DELETE (instead of SET) with a NULL value, but it's still
 * two extra parameters.  The other source of complexity is the "index"
 * parameter, which needs to be passed both in and out.  Normally that would
 * mean one parameter passed by reference, but there's no obvious sentinel
 * value on input, so we need at least one more bit to indicate that no index
 * should be passed to the server.  On the other end, we need at least one bit
 * to indicate that no index should be passed *back* to the caller.  So yes,
 * we can pass by reference, but then we also need a fourth extra flag
 * parameter with ETCD_INDEX_IN and ETCD_INDEX_OUT flags to narrow things down.
 *
 * For now, people can just poll.  If you really want etcd_watch implemented,
 * let me know - or earn my undying gratitude if you add it yourself.  ;)
 */
