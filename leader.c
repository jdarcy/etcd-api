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
#include <limits.h>
#include <regex.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <sys/types.h>

#include "etcd-api.h"

#define VOTE_ELEMS      4       /* Whole match plus three actual pieces. */
#define DEFAULT_FITNESS 42
#define DEFAULT_KEY     "nsr"
#define LEADER_TTL      5       /* TBD: make this tunable */

enum { NO_LEADER, TENTATIVE, CONFIRMED } leader_state_t;

regex_t vote_re;
char    *me;
long    my_fitness;
int     i_am_leader;

void
LeaderCallback (void)
{
        printf ("Bow down before me!\n");
}

int
GetCurrentLeader (etcd_session etcd, char *key)
{
        char            *text   = NULL;
        int             res;
        regmatch_t      matches[VOTE_ELEMS];
        char            *nominee;
        long            state;
        long            fitness;
        char            *vote   = NULL;
        int             retval  = EXIT_FAILURE;
        int             valid_text;

        for (;;) {
                
                if (text) {
                        free(text);
                }
                
                text = etcd_get(etcd,key);
                if (text) {
                        if (regexec(&vote_re,text,VOTE_ELEMS,matches,0) != 0) {
                                fprintf (stderr, "%s: got malformed vote %s\n",
                                         __func__, text);
                                break;
                        }
                        /* We can be destructive here, so convert commas. */
                        text[matches[1].rm_eo] = '\0';
                        text[matches[2].rm_eo] = '\0';
                        nominee = text + matches[1].rm_so;
                        state = strtol(text+matches[2].rm_so,NULL,10);
                        fitness = strtol(text+matches[3].rm_so,NULL,10);
                }
                else {
                        nominee = NULL;
                        state = NO_LEADER;
                        fitness = 0;
                }

                if (state == CONFIRMED) {
                        printf ("leader is %s\n",nominee);
                        if (strcmp(nominee,me) == 0) {
                                LeaderCallback();
                                i_am_leader = 1;
                        }
                        else {
                                i_am_leader = 0;
                        }
                        retval = EXIT_SUCCESS;
                        break;
                }

                /* TBD: override based on fitness */
                if ((state >= TENTATIVE) && (strcmp(nominee,me) != 0)) {
                        sleep(1);
                        continue;
                }

                if (vote) {
                        free(vote);
                }

                if (asprintf(&vote,"%s,%ld,%ld",me,state+1,my_fitness) < 0) {
                        fprintf (stderr, "%s: failed to construct vote\n",
                                 __func__);
                        break;
                }

                if (text) {
                        text[matches[1].rm_eo] = ',';
                        text[matches[2].rm_eo] = ',';
                }
                if (etcd_set(etcd,key,vote,text,LEADER_TTL) != ETCD_OK) {
                        fprintf (stderr, "%s: failed to cast vote\n",
                                 __func__);
                        break;
                }
                
                /* TBD: early return if we just went CONFIRMED? */
                sleep(1);
        }

        if (text) {
                free(text);
        }
        if (vote) {
                free(vote);
        }
        return retval;
}

int
Confirm (etcd_session etcd, char *key)
{
        char    *vote;

        if (asprintf(&vote,"%s,%ld,%ld",me,CONFIRMED,my_fitness) < 0) {
                fprintf (stderr, "%s: failed to construct confirmation\n",
                         __func__);
                return EXIT_FAILURE;
        }

        if (etcd_set(etcd,key,vote,vote,LEADER_TTL) != ETCD_OK) {
                fprintf (stderr, "%s: failed to confirm\n", __func__);
                free(vote);
                return EXIT_FAILURE;
        }
        
        free(vote);
        return EXIT_SUCCESS;
}

int
WatchLeaderChanges (etcd_session etcd, char *key)
{
#if 0
        char    *nkey;
        char    *nval;

        if (etcd_watch(etcd,key,&nkey,&nval,NULL,NULL) != ETCD_OK) {
                fprintf (stderr, "%s: etcd_watch failed\n", __func__);
                return EXIT_FAILURE;
        }

        if (nval) {
                printf ("new value = %s\n",nval);
                free(nval);
        }
        else {
                printf("value deleted\n");
        }
        free(nkey);
#endif

        return EXIT_SUCCESS;
}

struct option my_opts[] = {
        { "fitness",    required_argument,      NULL,   'f' },
        { "help",       no_argument,            NULL,   'h' },
        { "key",        required_argument,      NULL,   'k' },
        { "nodename",   required_argument,      NULL,   'n' },
        { "servers",    required_argument,      NULL,   's' },
        { NULL }
};

int
print_usage (char *prog)
{
        fprintf (stderr, "Usage: %s [options]\n", prog);
        fprintf (stderr, "  -f|--fitness  FFF  default %d\n", DEFAULT_FITNESS);
        fprintf (stderr, "  -k|--key      KKK  default %s\n", DEFAULT_KEY);
        fprintf (stderr, "  -n|--nodename NNN  default gethostname()\n");
        fprintf (stderr, "  -s|--servers  SSS  default $ETCD_SERVERS\n");
        return EXIT_FAILURE;
}

int
main (int argc, char **argv)
{
        etcd_session    etcd;
        int             opt;
        char            *key;
        char            hostname_raw[HOST_NAME_MAX+1];
        char            *server_list;

        /* Set defaults. */
        my_fitness = 42;
        key = DEFAULT_KEY;
        if (gethostname(hostname_raw,HOST_NAME_MAX) != 0) {
                fprintf (stderr, "failed to get host name\n");
                return EXIT_FAILURE;
        }
        hostname_raw[HOST_NAME_MAX] = '\0';
        me = hostname_raw;
        server_list = getenv("ETCD_SERVERS");

        /* TBD: better argument processing */
        for (;;) {
                opt = getopt_long(argc,argv,"f:hk:n:s:",my_opts,NULL);
                if (opt == (-1)) {
                        break;
                }
                switch (opt) {
                case 'f':
                        my_fitness = strtol(optarg,NULL,10);
                        break;
                case 'h':
                        return print_usage(argv[0]);
                case 'k':
                        key = optarg;
                        break;
                case 'n':
                        me = optarg;
                        break;
                case 's':
                        server_list = optarg;
                        break;
                default:
                        return print_usage(argv[0]);
                }
        }

        if (!server_list) {
                return print_usage(argv[0]);
        }

        if (regcomp(&vote_re,"([^,]+),([^,]+),([^,]+)",REG_EXTENDED) != 0) {
                fprintf (stderr, "Failed to set up vote regex\n");
                return EXIT_FAILURE;
        }

        etcd = etcd_open_str(server_list);
        if (!etcd) {
                fprintf (stderr, "Failed to open etcd session\n");
                return EXIT_FAILURE;
        }

        for (;;) {
                if (GetCurrentLeader(etcd,key) != EXIT_SUCCESS) {
                        break;
                }
                if (i_am_leader) {
                        do {
                                sleep(1);
                        } while (Confirm(etcd,key) == EXIT_SUCCESS);
                }
                else {
                        /*
                         * This doesn't really work, because an update to
                         * refresh the TTL will generate an event even if the
                         * value remains unchanged.  Even if that weren't the
                         * case, issuing a watch without having the last index
                         * in hand would be racy; something could change after
                         * we called GET in GetCurrentLeader but before we
                         * call WATCH in WatchLeaderChanges, and we'd miss the
                         * notification.  When etcd supports a TTL refresh that
                         * doesn't generate an event, and our API supports
                         * returning an index along with a value from GET, then
                         * we can enable this.  Until then it's just a no-op
                         * placeholder.
                         */
                        if (WatchLeaderChanges(etcd,key) != EXIT_SUCCESS) {
                                break;
                        }
                }
        }

        etcd_close_str(etcd);
        return EXIT_SUCCESS;
}
