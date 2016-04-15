//
// PRXY 0.2 20160415 (friday) copyright sk4zuzu@gmail.com 2016
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

#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <poll.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>


#define _fail_if(__cond__,__code__,__verb__)            \
    if (__cond__) {                                     \
        if (__verb__) printf("%s():%s:%s\n", __func__,  \
                                            #__cond__,  \
                               (char*)strerror(errno)); \
        __code__                                        \
        exit(errno);                                    \
    }

typedef struct {
    int   port;
    char *ipv4;
    int   dport;
    char *dipv4;
    int   verb;
} cfg_t;

typedef void (*fun_t) (int acpt, cfg_t *cfg);


static /*conn*/ int _connect(int acpt, cfg_t *cfg) {
    int rslt;

    int conn = socket(AF_INET, SOCK_STREAM, 0);
    _fail_if(conn < 0, close(acpt);, cfg->verb);

    struct sockaddr_in addr = {
        .sin_family      = AF_INET,
        .sin_port        = htons(cfg->dport),
        .sin_addr.s_addr = inet_addr(cfg->dipv4), 
    };

    rslt = connect(conn, (struct sockaddr *)&addr, sizeof(addr));
    _fail_if(rslt < 0, close(conn);
                       close(acpt);, cfg->verb);

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
                           close(conn);, cfg->verb);

        if (fds[0].revents & POLLIN) {
            char buf0 [8*1024 + 1];

            rslt = read(acpt, buf0, sizeof(buf0) - 1);
            _fail_if(rslt <= 0, close(conn);, cfg->verb);

            buf0[rslt] = 0;
            if (cfg->verb) printf(buf0[rslt - 1] == '\n' ? "%s" : "%s\n", buf0);

            rslt = write(conn, buf0, rslt);
            _fail_if(rslt <= 0, close(acpt);, cfg->verb);
        }

        if (fds[1].revents & POLLIN) {
            char buf1 [8*1024 + 1];

            rslt = read(conn, buf1, sizeof(buf1) - 1);
            _fail_if(rslt <= 0, close(acpt);, cfg->verb);

            buf1[rslt] = 0;
            if (cfg->verb) printf(buf1[rslt - 1] == '\n' ? "%s" : "%s\n", buf1);

            rslt = write(acpt, buf1, rslt);
            _fail_if(rslt <= 0, close(conn);, cfg->verb);
        }

        _fail_if(fds[0].revents & POLLHUP, close(conn);, cfg->verb);
        _fail_if(fds[1].revents & POLLHUP, close(acpt);, cfg->verb);
    }
}


static void _handler(int acpt, cfg_t *cfg) {
    _proxy(acpt, _connect(acpt, cfg), cfg);
}


static void spawn(fun_t fun, int srve, int acpt, cfg_t *cfg) {
    pid_t pid = fork();
    
    _fail_if(pid < 0,, cfg->verb);

    if (pid == 0) { close(srve); fun(acpt, cfg); exit(0); }
}


static void serve(cfg_t *cfg) {
    int rslt;

    int srve = socket(AF_INET, SOCK_STREAM, 0);
    _fail_if(srve < 0,, cfg->verb);

    int optn = 1;
    rslt = setsockopt(srve, SOL_SOCKET, SO_REUSEADDR, &optn, sizeof(optn));
    _fail_if(rslt < 0, close(srve);, cfg->verb);

    struct sockaddr_in addr = { .sin_family = AF_INET,
                                .sin_port   = htons(cfg->port),
                                .sin_addr   = inet_addr(cfg->ipv4), };

    rslt = bind(srve, (struct sockaddr *)&addr, sizeof(addr));
    _fail_if(rslt < 0, close(srve);, cfg->verb);

    rslt = listen(srve, 8);
    _fail_if(rslt < 0, close(srve);, cfg->verb);

    for (;;) {
        int acpt = accept(srve, NULL, NULL);
        _fail_if(acpt < 0, close(srve);, cfg->verb);

        spawn(_handler, srve, acpt, cfg);
        close(acpt);
    }
}


static void _usage(char *argv[]) {
    printf("Usage: %s [-h] [-s] [-B bind_ipv4]"
                              " [-b bind_port]"
                              " [-D dest_ipv4]"
                              " [-d dest_port]\n", argv[0]);
}


int main(int argc, char *argv[]) {
    cfg_t cfg = { .port  = 6969,
                  .ipv4  = "0.0.0.0",
                  .dport = 8080,
                  .dipv4 = "127.0.0.1",
                  .verb  = 1 };
    int opt;
    
    while ((opt = getopt(argc, argv, "B:b:D:d:sh")) != -1) {
        switch (opt) {
            case 'B': cfg.ipv4  = optarg;       break;
            case 'b': cfg.port  = atoi(optarg); break;
            case 'D': cfg.dipv4 = optarg;       break;
            case 'd': cfg.dport = atoi(optarg); break;
            case 's': cfg.verb  = 0;            break;
            case 'h':
            default : _usage(argv); exit(0);
        }
    }

    if (cfg.verb) printf("listening on %s:%d\n", cfg.ipv4, cfg.port);
    if (cfg.verb) printf("connecting to %s:%d\n", cfg.dipv4, cfg.dport);

    serve(&cfg);
}


// vim:ts=4:sw=4:et:
