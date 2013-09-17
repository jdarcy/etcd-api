#include <stdio.h>
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
main (int argc, char **argv)
{
        etcd_session            sess;
        char                    buf[256];
        yajl_val                node;
        yajl_val                value;
        static const char       *path[] = { "value", NULL };
        
        sess = etcd_open(my_servers);
        if (!sess) {
                fprintf(stderr,"etcd_open failed\n");
                return -1;
        }

        memset(buf,0,sizeof(buf));
        if (etcd_get(sess,argv[1],buf,sizeof(buf)-1) < 0) {
                fprintf(stderr,"etcd_get failed\n");
                return -1;
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
