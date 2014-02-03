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

/* For asprintf */
#if !defined(_GNU_SOURCE)
#define _GNU_SOURCE
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <curl/curl.h>
#include <yajl/yajl_tree.h>
#include "etcd-api.h"


#define DEFAULT_ETCD_PORT       4001
#define SL_DELIM                "\n\r\t ,;"

typedef struct {
        etcd_server     *servers;
} _etcd_session;

typedef struct {
        char            *key;
        char            *value;
        int             *index_in;      /* pointer so NULL can be special */
        int             index_out;      /* NULL would be meaningless */
} etcd_watch_t;

typedef size_t curl_callback_t (void *, size_t, size_t, void *);

int             g_inited        = 0;
const char      *value_path[]   = { "node", "value", NULL };

/*
 * We only call this in case where it should be safe, but gcc doesn't know
 * that so we use this to shut it up.
 */
char *
MY_YAJL_GET_STRING (yajl_val x)
{
        char *y = YAJL_GET_STRING(x);

        return y ? y : "bogus";
}

#if defined(DEBUG)
void
print_curl_error (char *intro, CURLcode res)
{
        printf("%s: %s\n",intro,curl_easy_strerror(res));
}
#else
#define print_curl_error(intro,res)
#endif

 
etcd_session
etcd_open (etcd_server *server_list)
{
        _etcd_session   *session;

        if (!g_inited) {
                curl_global_init(CURL_GLOBAL_ALL);
                g_inited = 1;
        }

        session = malloc(sizeof(*session));
        if (!session) {
                return NULL;
        }

        /*
         * Some day we'll set up more persistent connections, and keep track
         * (via redirects) of which server is leader so that we can always
         * try it first.  For now we just push that to the individual request
         * functions, which do the most brain-dead thing that can work.
         */

        session->servers = server_list;
        return session;
}


void
etcd_close (etcd_session session)
{
        free(session);
}

/*
 * Looking directly at node->u.array seems terribly un-modular, but the YAJL
 * tree interface doesn't seem to have any exposed API for iterating over the
 * elements of an array.  I tried using yajl_tree_get with an index in the
 * path, either as a type-casted integer or as a string, but that didn't work.
 */
char *
parse_array_response (yajl_val node)
{
        size_t          i;
        yajl_val        item;
        yajl_val        value;
        char            *retval = NULL;
        char            *saved;

        for (i = 0; i < node->u.array.len; ++i) {
                item = node->u.array.values[i];
                if (!item) {
                        break;
                }
                value = yajl_tree_get(item,value_path,yajl_t_string);
                if (!value) {
                        break;
                }
                if (retval) {
                        saved = retval;
                        retval = NULL;
                        (void)asprintf (&retval, "%s\n%s",
                                        saved, MY_YAJL_GET_STRING(value));
                        free(saved);
                }
                else {
                        retval = strdup(MY_YAJL_GET_STRING(value));
                }
                if (!retval) {
                        break;
                }
        }

        return retval;
}

size_t
parse_get_response (void *ptr, size_t size, size_t nmemb, void *stream)
{
        yajl_val        node;
        yajl_val        value;

        node = yajl_tree_parse(ptr,NULL,0);
        if (node) switch (node->type) {
        case yajl_t_object:
                value = yajl_tree_get(node,value_path,yajl_t_string);
                if (value) {
                        /* 
                         * YAJL probably copied it once, now we're going to
                         * copy it again.  If anybody really cares for such
                         * small and infrequently used values, we'd have to do
                         * do something much more complicated (like using the
                         * stream interface) to avoid the copy.  Right now it's
                         * just not worth it.
                         */
                        *((char **)stream) = strdup(MY_YAJL_GET_STRING(value));
                }
                break;
        case yajl_t_array:
                *((char **)stream) = parse_array_response(node);
                break;
        default:
                ;
        }

        yajl_tree_free(node);
        return size*nmemb;
}


etcd_result
etcd_get_one (_etcd_session *session, char *key, etcd_server *srv, char *prefix,
              char *post, curl_callback_t cb, char **stream)
{
        char            *url;
        CURL            *curl;
        CURLcode        curl_res;
        etcd_result     res             = ETCD_WTF;
        void            *err_label      = &&done;

        if (asprintf(&url,"http://%s:%u/v2/%s%s",
                     srv->host,srv->port,prefix,key) < 0) {
                goto *err_label;
        }
        err_label = &&free_url;

        curl = curl_easy_init();
        if (!curl) {
                goto *err_label;
        }
        err_label = &&cleanup_curl;

        /* TBD: add error checking for these */
        curl_easy_setopt(curl,CURLOPT_URL,url);
        curl_easy_setopt(curl,CURLOPT_FOLLOWLOCATION,1L);
        curl_easy_setopt(curl,CURLOPT_WRITEFUNCTION,cb);
        curl_easy_setopt(curl,CURLOPT_WRITEDATA,stream);
        if (post) {
                curl_easy_setopt(curl,CURLOPT_POST,1L);
                curl_easy_setopt(curl,CURLOPT_POSTFIELDS,post);
        }
#if defined(DEBUG)
        curl_easy_setopt(curl,CURLOPT_VERBOSE,1L);
#endif

        curl_res = curl_easy_perform(curl);
        if (curl_res != CURLE_OK) {
                print_curl_error("perform",curl_res);
                goto *err_label;
        }

        res = ETCD_OK;

cleanup_curl:
        curl_easy_cleanup(curl);
free_url:
        free(url);
done:
        return res;
}


char *
etcd_get (etcd_session session_as_void, char *key)
{
        _etcd_session   *session   = session_as_void;
        etcd_server     *srv;
        etcd_result     res;
        char            *value  = NULL;

        for (srv = session->servers; srv->host; ++srv) {
                res = etcd_get_one(session,key,srv,"keys/",NULL,
                                   parse_get_response,&value);
                if ((res == ETCD_OK) && value) {
                        return value;
                }
        }

        return NULL;
}


size_t
parse_watch_response (void *ptr, size_t size, size_t nmemb, void *stream)
{
        yajl_val                node;
        yajl_val                value;
        etcd_watch_t            *watch  = stream;
        static const char       *i_path[] = { "index", NULL };
        static const char       *k_path[] = { "key", NULL };
        static const char       *v_path[] = { "value", NULL };

        node = yajl_tree_parse(ptr,NULL,0);
        if (node) {
                value = yajl_tree_get(node,i_path,yajl_t_number);
                if (value) {
                        watch->index_out = strtoul(YAJL_GET_NUMBER(value),
                                                   NULL,10);
                }
                value = yajl_tree_get(node,k_path,yajl_t_string);
                if (value) {
                        watch->key = strdup(MY_YAJL_GET_STRING(value));
                }
                value = yajl_tree_get(node,v_path,yajl_t_string);
                if (value) {
                        watch->value = strdup(MY_YAJL_GET_STRING(value));
                }
                else {
                        /* Must have been a DELETE. */
                        watch->value = NULL;
                }
        }

        return size*nmemb;
}


etcd_result
etcd_watch (etcd_session session_as_void, char *pfx,
            char **keyp, char **valuep, int *index_in, int *index_out)
{
        _etcd_session   *session   = session_as_void;
        etcd_server     *srv;
        etcd_result     res;
        etcd_watch_t    watch;
        char            *post;

        if (index_in) {
                if (asprintf(&post,"index=%d",*index_in) < 0) {
                        return ETCD_WTF;
                }
        }
        else {
                post = NULL;
        }

        memset(&watch.key,0,sizeof(watch));
        watch.index_in = index_in;

        for (srv = session->servers; srv->host; ++srv) {
                res = etcd_get_one(session,pfx,srv,"watch/",post,
                                   parse_watch_response,(char **)&watch);
                if ((res == ETCD_OK) && watch.key) {
                        if (keyp) {
                                *keyp = watch.key;
                        }
                        if (valuep) {
                                *valuep = watch.value;
                        }
                        if (index_out) {
                                *index_out = watch.index_out;
                        }
                        break;
                }
        }

        if (post) {
                free(post);
        }
        return res;
}


size_t
parse_set_response (void *ptr, size_t size, size_t nmemb, void *stream)
{
        yajl_val        node;
        yajl_val        value;
        etcd_result     res     = ETCD_PROTOCOL_ERROR;
        /*
         * Success responses contain prevValue and index.  Failure responses
         * contain errorCode and cause.  Among all these, index seems to be the
         * one we're most likely to need later, so look for that.
         */
        static const char       *path[] = { "node", "modifiedIndex", NULL };

        node = yajl_tree_parse(ptr,NULL,0);
        if (node) {
                value = yajl_tree_get(node,path,yajl_t_number);
                if (value) {
                        res = ETCD_OK;
                }
        }

        *((etcd_result *)stream) = res;
        return size*nmemb;
}


/* NB: a null value means to use HTTP DELETE and ignore precond/ttl */
etcd_result
etcd_put_one (_etcd_session *session, char *key, char *value,
              char *precond, unsigned int ttl, etcd_server *srv)
{
        char                    *url;
        char                    *contents       = NULL;
        CURL                    *curl;
        etcd_result             res             = ETCD_WTF;
        CURLcode                curl_res;
        void                    *err_label      = &&done;

        if (asprintf(&url,"http://%s:%u/v2/keys/%s",
                     srv->host,srv->port,key) < 0) {
                goto *err_label;
        }
        err_label = &&free_url;

        if (value) {
                if (asprintf(&contents,"value=%s",value) < 0) {
                        goto *err_label;
                }
                err_label = &&free_contents;

                if (precond) {
                        char *c2;
                        if (asprintf(&c2,"%s;prevValue=%s",contents,
                                     precond) < 0) {
                                goto *err_label;
                        }
                        free(contents);
                        contents = c2;
                }

                if (ttl) {
                        char *c2;
                        if (asprintf(&c2,"%s;ttl=%u",contents,ttl) < 0) {
                                goto *err_label;
                        }
                        free(contents);
                        contents = c2;
                }
        }

        curl = curl_easy_init();
        if (!curl) {
                goto *err_label;
        }
        err_label = &&cleanup_curl;

        /* TBD: add error checking for these */
        curl_easy_setopt(curl,CURLOPT_CUSTOMREQUEST,"PUT");
        curl_easy_setopt(curl,CURLOPT_URL,url);
        curl_easy_setopt(curl,CURLOPT_FOLLOWLOCATION,1L);
        curl_easy_setopt(curl,CURLOPT_POSTREDIR,CURL_REDIR_POST_ALL);
        curl_easy_setopt(curl,CURLOPT_WRITEFUNCTION,parse_set_response);
        curl_easy_setopt(curl,CURLOPT_WRITEDATA,&res);
        if (value) {
                /*
                 * CURLOPT_HTTPPOST would be easier, but it looks like etcd
                 * will barf on that.  Sigh.
                 */
                curl_easy_setopt(curl,CURLOPT_POST,1L);
                curl_easy_setopt(curl,CURLOPT_POSTFIELDS,contents);
        }
        else {
                /* This must be a DELETE. */
                curl_easy_setopt(curl,CURLOPT_CUSTOMREQUEST,"DELETE");
        }
#if defined(DEBUG)
        curl_easy_setopt(curl,CURLOPT_VERBOSE,1L);
#endif

        curl_res = curl_easy_perform(curl);
        if (curl_res != CURLE_OK) {
                print_curl_error("perform",curl_res);
                goto *err_label;
        }

        /*
         * If the request succeeded, or at least got to the server and failed
         * there, parse_set_response should have set res appropriately.
         */

cleanup_curl:
        curl_easy_cleanup(curl);
free_contents:
        free(contents); /* might already be NULL for delete, but that's OK */
free_url:
        free(url);
done:
        return res;
}


etcd_result
etcd_set (etcd_session session_as_void, char *key, char *value,
          char *precond, unsigned int ttl)
{
        _etcd_session   *session   = session_as_void;
        etcd_server     *srv;
        etcd_result     res;

        for (srv = session->servers; srv->host; ++srv) {
                res = etcd_put_one(session,key,value,precond,ttl,srv);
                /*
                 * Protocol errors are likely to be things like precondition
                 * failures, which won't be helped by retrying on another
                 * server.
                 */
                if ((res == ETCD_OK) || (res == ETCD_PROTOCOL_ERROR)) {
                        return res;
                }
        }

        return ETCD_WTF;
}


/*
 * This uses the same path and status checks as SET, but with a different HTTP
 * command instead of data.  Precondition and TTL are obviously not used in
 * this case, though a conditional delete would be a cool feature for etcd.  I
 * think you can get a timed delete by doing a conditional set to the current
 * value with a TTL, but I haven't actually tried it.
 */
etcd_result
etcd_delete (etcd_session session_as_void, char *key)
{
        _etcd_session   *session   = session_as_void;
        etcd_server     *srv;
        etcd_result     res;

        for (srv = session->servers; srv->host; ++srv) {
                res = etcd_put_one(session,key,NULL,NULL,0,srv);
                if (res == ETCD_OK) {
                        break;
                }
        }

        return res;
}


size_t
store_leader (void *ptr, size_t size, size_t nmemb, void *stream)
{
        *((char **)stream) = strdup(ptr);
        return size * nmemb;
}


char *
etcd_leader (etcd_session session_as_void)
{
        _etcd_session   *session   = session_as_void;
        etcd_server     *srv;
        etcd_result     res;
        char            *value  = NULL;

        for (srv = session->servers; srv->host; ++srv) {
                res = etcd_get_one(session,"leader",srv,"",NULL,
                                   store_leader,&value);
                if ((res == ETCD_OK) && value) {
                        return value;
                }
        }

        return NULL;
}


void
free_sl (etcd_server *server_list)
{
        size_t          num_servers;

        for (num_servers = 0; server_list[num_servers].host; ++num_servers) {
                free(server_list[num_servers].host);
        }
        free(server_list);
}


int
_count_matching (char *text, char *cset, int result)
{
        char    *t;
        int     res     = 0;

        for (t = text; *t; ++t) {
                if ((strchr(cset,*t) != NULL) != result) {
                        break;
                }
                ++res;
        }

        return res;
}

#define count_matching(t,cs)    _count_matching(t,cs,1)
#define count_nonmatching(t,cs) _count_matching(t,cs,0)


etcd_session
etcd_open_str (char *server_names)
{
        char            *snp;
        int             run_len;
        int             host_len;
        size_t           num_servers;
        etcd_server     *server_list;
        etcd_session    *session;

        /*
         * Yeah, we iterate over the string twice so we can allocate an
         * appropriately sized array instead of turning it into a linked list.
         * Unfortunately this means we can't use strtok* which is destructive
         * with no platform-independent way to reverse the destructive effects.
         */

        num_servers = 0;
        snp = server_names;
        while (*snp) {
                run_len = count_nonmatching(snp,SL_DELIM);
                if (!run_len) {
                        snp += count_matching(snp,SL_DELIM);
                        continue;
                }
                ++num_servers;
                snp += run_len;
        }

        if (!num_servers) {
                return NULL;
        }

        server_list = calloc(num_servers+1,sizeof(*server_list));
        if (!server_list) {
                return NULL;
        }
        num_servers = 0;

        snp = server_names;
        while (*snp) {
                run_len = count_nonmatching(snp,SL_DELIM);
                if (!run_len) {
                        snp += count_matching(snp,SL_DELIM);
                        continue;
                }
                host_len = count_nonmatching(snp,":");
                if ((run_len - host_len) > 1) {
                        server_list[num_servers].host = strndup(snp,host_len);
                        server_list[num_servers].port = (unsigned short)
                                strtoul(snp+host_len+1,NULL,10);
                }
                else {
                        server_list[num_servers].host = strndup(snp,run_len);
                        server_list[num_servers].port = DEFAULT_ETCD_PORT;
                }
                ++num_servers;
                snp += run_len;
        }

        session = etcd_open(server_list);
        if (!session) {
                free_sl(server_list);
        }
        return session;
}


void
etcd_close_str (etcd_session session)
{
        free_sl(((_etcd_session *)session)->servers);
        etcd_close(session);
}
