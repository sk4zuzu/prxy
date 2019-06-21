//
// PRXY 0.4 20190621 (friday) copyright sk4zuzu@gmail.com 2019
//
// This file is part of PRXY.
//
// PRXY is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// PRXY is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with PRXY.  If not, see <http://www.gnu.org/licenses/>.
//

#include <inttypes.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <poll.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <ares.h>


#define _fail_if(__cond__,__code__)                      \
    if (__cond__) {                                      \
        if (cfg->verb) printf("%s():%s:%s\n", __func__,  \
                                             #__cond__,  \
                                (char*)strerror(errno)); \
        __code__                                         \
        exit(errno);                                     \
    }

typedef struct {
    int   port;
    char *ipv4;
    int   dport;
    char *dhost;
    char  dhost_resolved[INET6_ADDRSTRLEN];
    int   verb;
    int   count;
} cfg_t;

typedef void (*fun_t) (int acpt, cfg_t *cfg);


static void _resolve(void *arg, int status, int timeouts, struct hostent *hostent) {
    cfg_t *cfg = (cfg_t*) arg;

    if (status == ARES_SUCCESS) {
        inet_ntop(hostent->h_addrtype, hostent->h_addr_list[0], (char*) &cfg->dhost_resolved, sizeof(cfg->dhost_resolved));
        cfg->dhost = (char*) &cfg->dhost_resolved;
    }
}


static void resolve(cfg_t *cfg) {
    int rslt;

    rslt = ares_library_init(ARES_LIB_INIT_ALL);
    _fail_if(rslt != ARES_SUCCESS,);

    ares_channel channel;

    rslt = ares_init(&channel);
    _fail_if(rslt != ARES_SUCCESS,);

    ares_gethostbyname(channel, cfg->dhost, AF_UNSPEC, _resolve, (void*) cfg);

    for (;;) {
        fd_set read_fds, write_fds;

        FD_ZERO(&read_fds);
        FD_ZERO(&write_fds);

        int nfds = ares_fds(channel, &read_fds, &write_fds);
        if (nfds == 0) {
            break;
        }

        struct timeval tv;

        rslt = select(nfds, &read_fds, &write_fds, NULL, ares_timeout(channel, NULL, &tv));
        _fail_if(rslt < 0, ares_destroy(channel);
                           ares_library_cleanup(); );

        ares_process(channel, &read_fds, &write_fds);
    }

    ares_destroy(channel);
    ares_library_cleanup();
}


static /*conn*/ int _connect(int acpt, cfg_t *cfg) {
    int rslt;

    int conn = socket(AF_INET, SOCK_STREAM, 0);
    _fail_if(conn < 0, close(acpt); );

    struct sockaddr_in addr = {
        .sin_family      = AF_INET,
        .sin_port        = htons(cfg->dport),
        .sin_addr.s_addr = inet_addr(cfg->dhost),
    };

    rslt = connect(conn, (struct sockaddr *)&addr, sizeof(addr));
    _fail_if(rslt < 0, close(conn);
                       close(acpt); );

    return conn;
}


static void _proxy(int acpt, int conn, cfg_t *cfg) {
    struct pollfd fds[2] = {
        { .fd      = acpt,
          .events  = POLLIN,
          .revents = 0, },
        { .fd      = conn,
          .events  = POLLIN,
          .revents = 0, },
    };

    for (;;) {
        int rslt = poll(fds, 2, -1);

        _fail_if(rslt < 0, close(acpt);
                           close(conn); );

        if (fds[0].revents & POLLIN) {
            char buf0 [8*1024 + 1];

            rslt = read(acpt, buf0, sizeof(buf0) - 1);
            _fail_if(rslt <= 0, close(conn); );

            buf0[rslt] = 0;
            if (cfg->verb) {
                printf(buf0[rslt - 1] == '\n' ? "%s" : "%s\n", buf0);
                fflush(stdout);
            }

            rslt = write(conn, buf0, rslt);
            _fail_if(rslt <= 0, close(acpt); );
        }

        if (fds[1].revents & POLLIN) {
            char buf1 [8*1024 + 1];

            rslt = read(conn, buf1, sizeof(buf1) - 1);
            _fail_if(rslt <= 0, close(acpt); );

            buf1[rslt] = 0;
            if (cfg->verb) {
                printf(buf1[rslt - 1] == '\n' ? "%s" : "%s\n", buf1);
                fflush(stdout);
            }

            rslt = write(acpt, buf1, rslt);
            _fail_if(rslt <= 0, close(conn); );
        }

        _fail_if(fds[0].revents & POLLHUP, close(conn); );
        _fail_if(fds[1].revents & POLLHUP, close(acpt); );
    }
}


static void _handler(int acpt, cfg_t *cfg) {
    _proxy(acpt, _connect(acpt, cfg), cfg);
}


static void spawn(fun_t fun, int srve, int acpt, cfg_t *cfg) {
    pid_t pid = fork();

    _fail_if(pid < 0,);

    if (pid == 0) { close(srve); fun(acpt, cfg); exit(0); }
}


static void serve(cfg_t *cfg) {
    int rslt;

    int srve = socket(AF_INET, SOCK_STREAM, 0);
    _fail_if(srve < 0,);

    int optn = 1;
    rslt = setsockopt(srve, SOL_SOCKET, SO_REUSEADDR, &optn, sizeof(optn));
    _fail_if(rslt < 0, close(srve); );

    struct sockaddr_in addr = { .sin_family = AF_INET,
                                .sin_port   = htons(cfg->port),
                                .sin_addr   = inet_addr(cfg->ipv4), };

    rslt = bind(srve, (struct sockaddr *)&addr, sizeof(addr));
    _fail_if(rslt < 0, close(srve); );

    rslt = listen(srve, 8);
    _fail_if(rslt < 0, close(srve); );

    uintmax_t counter = 0;

    for (;;) {
        int acpt = accept(srve, NULL, NULL);
        _fail_if(acpt < 0, close(srve); );

        if (cfg->count) {
            counter++;
            printf("cc: %"PRIuMAX"\n", counter);
            fflush(stdout);
        }

        spawn(_handler, srve, acpt, cfg);
        close(acpt);
    }
}


static void _usage(char *argv[]) {
    printf("Usage: %s [-h] [-s] [-c] [-B bind_ipv4]"
                                   " [-b bind_port]"
                                   " [-D dest_host]"
                                   " [-d dest_port]\n", argv[0]);
}


int main(int argc, char *argv[]) {
    cfg_t cfg = { .port           = 6969,
                  .ipv4           = "0.0.0.0",
                  .dport          = 8080,
                  .dhost          = "127.0.0.1",
                  .dhost_resolved = "",
                  .verb           = 1,
                  .count          = 0 };
    int opt;

    while ((opt = getopt(argc, argv, "B:b:D:d:csh")) != -1) {
        switch (opt) {
            case 'B': cfg.ipv4  = optarg;       break;
            case 'b': cfg.port  = atoi(optarg); break;
            case 'D': cfg.dhost = optarg;       break;
            case 'd': cfg.dport = atoi(optarg); break;
            case 'c': cfg.count = 1;            break;
            case 's': cfg.verb  = 0;            break;
            case 'h':
            default : _usage(argv); exit(0);
        }
    }

    resolve(&cfg);

    if (cfg.verb) {
        printf("listening on %s:%d\n", cfg.ipv4, cfg.port);
        printf("connecting to %s:%d\n", cfg.dhost, cfg.dport);
        fflush(stdout);
    }

    serve(&cfg);
}


// vim:ts=4:sw=4:et:syn=c:
