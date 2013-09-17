#include <stdio.h>
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
        etcd_session    sess;
        char            buf[256];
        
        sess = etcd_open(my_servers);
        if (!sess) {
                fprintf(stderr,"etcd_open failed\n");
                return -1;
        }

        if (etcd_get(sess,argv[1],buf,sizeof(buf)) < 0) {
                fprintf(stderr,"etcd_get failed\n");
                return -1;
        }

        printf("%.*s\n",sizeof(buf),buf);
        return 0;
}
