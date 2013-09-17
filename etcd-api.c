#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <curl/curl.h>
#include "etcd-api.h"


typedef struct {
        etcd_server     *servers;
} _etcd_session;

typedef struct {
        void            *ptr;
        size_t          max;
        size_t          res;
} etcd_buffer;

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
write_data (void *ptr, size_t size, size_t nmemb, void *stream)
{
        etcd_buffer     *ebuf   = stream;

        ebuf->res = size * nmemb;
        if (ebuf->res > ebuf->max) {
                ebuf->res = ebuf->max;
        }

        memcpy(ebuf->ptr,ptr,ebuf->res);
        return ebuf->res;
}


ssize_t
etcd_get_one (_etcd_session *this, char *key, void *buf, size_t len,
              etcd_server *srv)
{
        char            *url;
        CURL            *curl;
        CURLcode        res;
        ssize_t         n_read          = -1;
        void            *err_label      = &&done;
        etcd_buffer     ebuf;

        if (asprintf(&url,"http://%s:%u/v1/keys/%s",
                     srv->host,srv->port,key) < 0) {
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
        curl_easy_setopt(curl,CURLOPT_WRITEFUNCTION,write_data);

        ebuf.ptr = buf;
        ebuf.max = len;
        ebuf.res = 0;
        curl_easy_setopt(curl,CURLOPT_WRITEDATA,&ebuf);

        res = curl_easy_perform(curl);
        if (res != CURLE_OK) {
                print_curl_error("perform",res);
                goto *err_label;
        }

        n_read = ebuf.res;

cleanup_curl:
        curl_easy_cleanup(curl);
free_url:
        free(url);
done:
        return n_read;
}


ssize_t
etcd_get (etcd_session this_as_void, char *key, void *buf, size_t len)
{
        _etcd_session   *this   = this_as_void;
        etcd_server     *srv;
        ssize_t         n_read;

        for (srv = this->servers; srv->host; ++srv) {
                n_read = etcd_get_one(this,key,buf,len,srv);
                if (n_read >= 0) {
                        return n_read;
                }
        }

        return -1;
}
