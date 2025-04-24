CC = gcc
OUTPUT = prxy
OBJS = $(patsubst %.c,%.o, $(wildcard *.c))
INCS = -Ic-ares/include/ -Ic-ares/build/

C-ARES_VERSION = 1.34.5

.PHONY: all docker-build clean distclean

all: $(OUTPUT)

docker-build:
	docker build -t ghcr.io/sk4zuzu/prxy -f Dockerfile .

$(OUTPUT): $(OBJS) c-ares/build/lib/libcares.a
	$(CC) -static -o $@ $^

%.o: %.c c-ares/build/lib/libcares.a
	$(CC) -g -c $(INCS) -o $@ $<

c-ares/build/lib/libcares.a:
	(git clone --depth=1 -b v$(C-ARES_VERSION) https://github.com/c-ares/c-ares.git || true) \
	&& cd c-ares/ \
	&& install -d build/ \
	&& cd build/ \
	&& cmake -DCMAKE_INSTALL_LIBDIR=lib -DCARES_SHARED=OFF -DCARES_STATIC=ON -DCARES_STATIC_PIC=ON .. \
	&& make

clean:
	-rm c-ares/build/lib/libcares.a
	-rm $(OUTPUT) $(OBJS)

distclean: clean
	-rm -rf c-ares/
