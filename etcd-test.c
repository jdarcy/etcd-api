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

#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "etcd-api.h"


int
do_get (etcd_session sess, char *key)
{
        char            *value;       

        printf("getting %s\n",key);

        value = etcd_get(sess,key);
        if (!value) {
                fprintf(stderr,"etcd_get failed\n");
                return !0;
        }

        printf("got value: %s\n",value);
        free(value);
        return 0;
}


int
do_set (etcd_session sess, char *key, char *value, char *precond, char *ttl)
{
        unsigned long           ttl_num = 0;

        printf("setting %s to %s\n",key,value);
        if (precond) {
                printf("  precond = %s\n",precond);
        }
        if (ttl) {
                /*
                 * It probably seems a bit silly to convert from a string to
                 * number when we're going to do the exact opposite in
                 * etcd_set, but this is just a test program.  In real API
                 * usage we're more likely to have a number in hand.
                 */
                ttl_num = strtoul(ttl,NULL,10);
                printf("  ttl = %lu\n",ttl_num);
        }
        else {
                ttl_num = 0;
        }

        if (etcd_set(sess,key,value,precond,ttl_num) != ETCD_OK) {
                fprintf(stderr,"etcd_set failed\n");
                return !0;
        }

        return 0;
}


int
do_delete (etcd_session sess, char *key)
{
        printf("deleting %s\n",key);

        if (etcd_delete(sess,key) != ETCD_OK) {
                fprintf(stderr,"etcd_delete failed\n");
                return !0;
        }

        return 0;
}


int
do_leader (etcd_session sess)
{
        char            *value;       

        printf("finding leader\n");

        value = etcd_leader(sess);
        if (!value) {
                fprintf(stderr,"etcd_leader failed\n");
                return !0;
        }

        printf("leader is %s\n",value);
        free(value);
        return 0;
}


struct option my_opts[] = {
        { "delete",     no_argument,            NULL,   'd' },
        { "precond",    required_argument,      NULL,   'p' },
        { "servers",    required_argument,      NULL,   's' },
        { "ttl",        required_argument,      NULL,   't' },
        { NULL }
};

int
print_usage (char *prog)
{
        fprintf (stderr, "Usage: %s [-s server-list] ...\n",prog);
        fprintf (stderr, "  for get:    key\n");
        fprintf (stderr, "  for set:    [-p precond] [-t ttl] key value\n");
        fprintf (stderr, "  for delete: -d key\n");
        fprintf (stderr, "  for leader: (no cmd-args)\n");
        fprintf (stderr, "Server list is host:port pairs separated by comma,\n"
                         "semicolon, or white space.  If not given on the\n"
                         "command line, ETCD_SERVERS will be used from the\n"
                         "environment instead.\n");

        return !0;
}

int
main (int argc, char **argv)
{
        int             opt;
        int             delete          = 0;
        char            *precond        = NULL;
        char            *servers        = getenv("ETCD_SERVERS");
        char            *ttl            = NULL;
        etcd_session    sess;
        int             res;

        for (;;) {
                opt = getopt_long(argc,argv,"dp:s:t:",my_opts,NULL);
                if (opt == (-1)) {
                        break;
                }
                switch (opt) {
                case 'd':
                        delete = 1;
                        break;
                case 'p':
                        precond = optarg;
                        break;
                case 's':
                        servers = optarg;
                        break;
                case 't':
                        ttl = optarg;
                        break;
                default:
                        return print_usage(argv[0]);
                }
        }

        if (!servers) {
                return print_usage(argv[0]);
        }
        if (delete && ((argc - optind) != 1)) {
                return print_usage(argv[0]);
        }
        if ((precond || ttl) && ((argc - optind) != 2)) {
                return print_usage(argv[0]);
        }

        sess = etcd_open_str(strdup(servers));
        if (!sess) {
                fprintf(stderr,"etcd_open failed\n");
                return !0;
        }

        switch (argc - optind) {
        case 0:
                res = do_leader(sess);
                break;
        case 1:
                if (delete) {
                        res = do_delete(sess,argv[optind]);
                }
                else {
                        res = do_get(sess,argv[optind]);
                }
                break;
        case 2:
                res = do_set(sess,argv[optind],argv[optind+1],precond,ttl);
                break;
        default:
                return print_usage(argv[0]);
        }

        etcd_close_str(sess);
        return res;
}
