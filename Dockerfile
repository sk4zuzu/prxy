FROM alpine AS builder

RUN apk --no-cache add binutils cmake curl gcc git make musl-dev

COPY Makefile prxy.c /src/

WORKDIR /src/

RUN make && strip --strip-unneeded prxy

ARG TINI_VERSION=0.19.0

RUN curl -fsSL https://github.com/krallin/tini/releases/download/v${TINI_VERSION}/tini-static-amd64 \
  | install -m u=rwx,go=rx /dev/fd/0 /tini \
 && strip --strip-unneeded /tini

FROM alpine

RUN apk --no-cache add bash bat curl fd nmap-ncat ripgrep tcpdump vim

COPY --from=builder /src/prxy /usr/local/bin/
COPY --from=builder /tini     /tini

USER 1000

ENTRYPOINT ["/tini", "--"]
