#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <curl/curl.h>
#include <yajl/yajl_tree.h>
#include "etcd-api.h"


typedef struct {
        etcd_server     *servers;
} _etcd_session;

typedef size_t curl_callback_t (void *, size_t, size_t, void *);

int g_inited = 0;

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
        _etcd_session   *this;

        if (!g_inited) {
                curl_global_init(CURL_GLOBAL_ALL);
                g_inited = 1;
        }

        this = malloc(sizeof(*this));
        if (!this) {
                return NULL;
        }

        /*
         * Some day we'll set up more persistent connections, but for now
         * that work is pushed to other methods.
         */

        this->servers = server_list;
        return this;
}


void
etcd_close (etcd_session this)
{
        free(this);
}


size_t
parse_get_response (void *ptr, size_t size, size_t nmemb, void *stream)
{
        yajl_val        node;
        yajl_val        value;
        static const char       *path[] = { "value", NULL };

        node = yajl_tree_parse(ptr,NULL,0);
        if (node) {
                value = yajl_tree_get(node,path,yajl_t_string);
                if (value) {
                        /* 
                         * YAJL probably copied it once, now we're going to
                         * copy it again.  If anybody really cares for such
                         * small and infrequently used values, we'd have to do
                         * do something much more complicated (like using the
                         * stream interface) to avoid the copy.  Right now it's
                         * just not worth it.
                         */
                        *((char **)stream) = strdup(YAJL_GET_STRING(value));
                }
        }

        return size*nmemb;
}


char *
etcd_get_one (_etcd_session *this, char *key, etcd_server *srv,
              char *prefix, curl_callback_t cb)
{
        char            *url;
        CURL            *curl;
        CURLcode        curl_res;
        char            *res            = NULL;
        ssize_t         n_read          = -1;
        void            *err_label      = &&done;

        if (asprintf(&url,"http://%s:%u/v1/%s%s",
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
        curl_easy_setopt(curl,CURLOPT_WRITEDATA,&res);
        curl_easy_setopt(curl,CURLOPT_VERBOSE,1L);

        curl_res = curl_easy_perform(curl);
        if (curl_res != CURLE_OK) {
                print_curl_error("perform",curl_res);
                goto *err_label;
        }

        /*
         * If the request succeeded, parse_get_response should have set res for
         * us.
         */

cleanup_curl:
        curl_easy_cleanup(curl);
free_url:
        free(url);
done:
        return res;
}


char *
etcd_get (etcd_session this_as_void, char *key)
{
        _etcd_session   *this   = this_as_void;
        etcd_server     *srv;
        char            *res;

        for (srv = this->servers; srv->host; ++srv) {
                res = etcd_get_one(this,key,srv,"keys/",parse_get_response);
                if (res) {
                        return res;
                }
        }

        return NULL;
}


size_t
store_leader (void *ptr, size_t size, size_t nmemb, void *stream)
{
        *((char **)stream) = strdup(ptr);
        return size * nmemb;
}


char *
etcd_leader (etcd_session this_as_void)
{
        _etcd_session   *this   = this_as_void;
        etcd_server     *srv;
        char            *res;

        for (srv = this->servers; srv->host; ++srv) {
                res = etcd_get_one(this,"leader",srv,"",store_leader);
                if (res) {
                        return res;
                }
        }

        return NULL;
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
        static const char       *path[] = { "index", NULL };

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


etcd_result
etcd_put_one (_etcd_session *this, char *key, char *value,
              char *precond, unsigned int ttl, etcd_server *srv)
{
        char                    *url;
        char                    *contents;
        CURL                    *curl;
        etcd_result             res             = ETCD_WTF;
        CURLcode                curl_res;
        void                    *err_label      = &&done;

        if (asprintf(&url,"http://%s:%u/v1/keys/%s",
                     srv->host,srv->port,key) < 0) {
                goto *err_label;
        }
        err_label = &&free_url;

        if (asprintf(&contents,"value=%s",value) < 0) {
                goto *err_label;
        }
        err_label = &&free_contents;

        if (precond) {
                char *c2;
                if (asprintf(&c2,"%s;prevValue=%s",contents,precond) < 0) {
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

        curl = curl_easy_init();
        if (!curl) {
                goto *err_label;
        }
        err_label = &&cleanup_curl;

        /* TBD: add error checking for these */
        curl_easy_setopt(curl,CURLOPT_URL,url);
        curl_easy_setopt(curl,CURLOPT_FOLLOWLOCATION,1L);
        curl_easy_setopt(curl,CURLOPT_WRITEFUNCTION,parse_set_response);
        curl_easy_setopt(curl,CURLOPT_WRITEDATA,&res);
        curl_easy_setopt(curl,CURLOPT_VERBOSE,1L);

        /*
         * CURLOPT_HTTPPOST would be easier, but it looks like etcd will barf
         * on that.  Sigh.
         */
        curl_easy_setopt(curl,CURLOPT_POST,1L);
        curl_easy_setopt(curl,CURLOPT_POSTFIELDS,contents);

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
        free(contents);
free_url:
        free(url);
done:
        return res;
}


etcd_result
etcd_set (etcd_session this_as_void, char *key, char *value,
          char *precond, unsigned int ttl)
{
        _etcd_session   *this   = this_as_void;
        etcd_server     *srv;
        etcd_result     res;

        for (srv = this->servers; srv->host; ++srv) {
                res = etcd_put_one(this,key,value,precond,ttl,srv);
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
