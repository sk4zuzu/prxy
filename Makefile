CC = gcc
OUTPUT = prxy
OBJS = $(patsubst %.c,%.o, $(wildcard *.c))
INCS = -Ic-ares/ -Ic-ares/build/

C-ARES_VERSION = cares-1_15_0

.PHONY: all release clean distclean

all: $(OUTPUT)

release:
	docker build -t sk4zuzu/prxy -f Dockerfile . \
	&& docker push sk4zuzu/prxy

$(OUTPUT): $(OBJS) c-ares/build/lib/libcares.a
	$(CC) -static -o $@ $^

%.o: %.c c-ares/build/lib/libcares.a
	$(CC) -g -c $(INCS) -o $@ $<

c-ares/build/lib/libcares.a:
	(git clone --depth=1 -b $(C-ARES_VERSION) https://github.com/c-ares/c-ares.git || true) \
	&& cd c-ares/ \
	&& install -d build/ \
	&& cd build/ \
	&& cmake -DCARES_SHARED=OFF -DCARES_STATIC=ON -DCARES_STATIC_PIC=ON .. \
	&& make

clean:
	-rm c-ares/build/lib/libcares.a
	-rm $(OUTPUT) $(OBJS)

distclean: clean
	-rm -rf c-ares/
