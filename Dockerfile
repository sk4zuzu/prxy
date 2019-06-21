
FROM alpine as builder

RUN apk --no-cache add curl git \
 && apk --no-cache add binutils gcc make cmake musl-dev

COPY Makefile prxy.c /src/

RUN cd /src/ \
 && make \
 && strip --strip-unneeded prxy

ARG TINI_VERSION=0.18.0

RUN curl -fsSL https://github.com/krallin/tini/releases/download/v${TINI_VERSION}/tini-static-amd64 \
         -o /tini \
 && chmod +x /tini \
 && strip --strip-unneeded /tini

FROM alpine

COPY --from=builder /src/prxy /usr/local/bin/
COPY --from=builder /tini     /tini

ENTRYPOINT ["/tini", "--"]

# vim:ts=2:sw=2:et:syn=dockerfile:
