#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "etcd-api.h"

etcd_server my_servers[] = {
        { "gfs1",  4001 },
        { "gfs1",  4002 },
        { "gfs1",  4003 },
        { NULL }
};

int
do_leader (void)
{
        etcd_session    sess;
        char            *value;       

        printf("finding leader\n");

        sess = etcd_open(my_servers);
        if (!sess) {
                fprintf(stderr,"etcd_open failed\n");
                return !0;
        }

        value = etcd_leader(sess);
        if (!value) {
                fprintf(stderr,"etcd_leader failed\n");
                return !0;
        }

        printf("leader is %s\n",value);
        free(value);
        return 0;
}

int
do_get (char *key)
{
        etcd_session    sess;
        char            *value;       

        printf("getting %s\n",key);

        sess = etcd_open(my_servers);
        if (!sess) {
                fprintf(stderr,"etcd_open failed\n");
                return !0;
        }

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
do_set (char *key, char *value, char *precond, char *ttl)
{
        etcd_session            sess;
        unsigned long           ttl_num = 0;

        printf("setting %s to %s\n",key,value);
        if (precond) {
                printf("  precond = %s\n",precond);
        }
        if (ttl) {
                printf("  ttl = %s\n",ttl);
        }

        sess = etcd_open(my_servers);
        if (!sess) {
                fprintf(stderr,"etcd_open failed\n");
                return -1;
        }

        /*
         * It probably seems a bit silly to convert from a string to number
         * when we're going to do the exact opposite in etcd_set, but this is
         * just a test program.  In real API usage we're more likely to have
         * a number in hand.
         */
        ttl_num = strtoul(ttl,NULL,10);

        if (etcd_set(sess,key,value,precond,ttl_num) != ETCD_OK) {
                fprintf(stderr,"etcd_set failed\n");
                return !0;
        }

        return 0;
}


struct option my_opts[] = {
        { "precond",    required_argument,      NULL,   'p' },
        { "ttl",        required_argument,      NULL,   't' },
        { NULL }
};

int
print_usage (char *prog)
{
        fprintf (stderr, "Usage: %s # shows leader",prog);
        fprintf (stderr, "       %s get-key\n",prog);
        fprintf (stderr, "       %s [-p precond] [-t ttl] set-key value\n",
                 prog);
        return !0;
}

int
main (int argc, char **argv)
{
        int                     opt;
        char                    *precond        = NULL;
        char                    *ttl            = NULL;

        for (;;) {
                opt = getopt_long(argc,argv,"p:t:",my_opts,NULL);
                if (opt == (-1)) {
                        break;
                }
                switch (opt) {
                case 'p':
                        precond = optarg;
                        break;
                case 't':
                        ttl = optarg;
                        break;
                default:
                        return print_usage(argv[0]);
                }
        }

        switch (argc - optind) {
        case 0:
                if (precond || ttl) {
                        return print_usage(argv[0]);
                }
                return do_leader();
        case 1:
                if (precond || ttl) {
                        return print_usage(argv[0]);
                }
                return do_get(argv[optind]);
        case 2:
                return do_set(argv[optind],argv[optind+1],precond,ttl);
        default:
                return print_usage(argv[0]);
        }
        
}
