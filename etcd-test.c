#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <yajl/yajl_tree.h>
#include "etcd-api.h"

etcd_server my_servers[] = {
        { "localhost",  4001 },
        { "localhost",  4002 },
        { "localhost",  4003 },
        { NULL }
};

int
do_get (char *key)
{
        etcd_session            sess;
        char                    buf[256];
        yajl_val                node;
        yajl_val                value;
        static const char       *path[] = { "value", NULL };

        printf("getting %s\n",key);

        sess = etcd_open(my_servers);
        if (!sess) {
                fprintf(stderr,"etcd_open failed\n");
                return !0;
        }

        memset(buf,0,sizeof(buf));
        if (etcd_get(sess,key,buf,sizeof(buf)-1) < 0) {
                fprintf(stderr,"etcd_get failed\n");
                return !0;
        }

        node = yajl_tree_parse(buf,NULL,0);
        if (node) {
                value = yajl_tree_get(node,path,yajl_t_string);
                if (value) {
                        printf("value = %s\n",YAJL_GET_STRING(value));
                }
                yajl_tree_free(node);
        }

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
        fprintf (stderr, "Usage: %s get-key\n",prog);
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
