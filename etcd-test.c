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
        { "ttl",        required_argument,      NULL,   't' },
        { NULL }
};

int
print_usage (char *prog)
{
        fprintf (stderr, "get:    %s key\n", prog);
        fprintf (stderr, "set:    %s [-p precond] [-t ttl] key value\n",
                 prog);
        fprintf (stderr, "delete: %s -d key\n", prog);
        fprintf (stderr, "leader: %s\n", prog);
        return !0;
}

int
main (int argc, char **argv)
{
        int             opt;
        int             delete          = 0;
        char            *precond        = NULL;
        char            *ttl            = NULL;
        etcd_session    sess;

        for (;;) {
                opt = getopt_long(argc,argv,"dp:t:",my_opts,NULL);
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
                case 't':
                        ttl = optarg;
                        break;
                default:
                        return print_usage(argv[0]);
                }
        }

        if (delete && ((argc - optind) != 1)) {
                return print_usage(argv[0]);
        }
        if ((precond || ttl) && ((argc - optind) != 2)) {
                return print_usage(argv[0]);
        }

        sess = etcd_open(my_servers);
        if (!sess) {
                fprintf(stderr,"etcd_open failed\n");
                return !0;
        }

        switch (argc - optind) {
        case 0:
                return do_leader(sess);
        case 1:
                if (delete) {
                        return do_delete(sess,argv[optind]);
                }
                else {
                        return do_get(sess,argv[optind]);
                }
        case 2:
                return do_set(sess,argv[optind],argv[optind+1],precond,ttl);
        default:
                /* Shut up, gcc.  The real fall-through is at the end. */
                ;
        }

        return print_usage(argv[0]);
}
