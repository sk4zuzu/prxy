CC=clang
OUTPUT=prxy
OBJS=$(patsubst %.c,%.o, $(wildcard *.c))

all: $(OUTPUT)

$(OUTPUT): $(OBJS) $(LIBS)
	$(CC) -o $@ $^

%.o: %.c
	$(CC) -g -c $(INCS) -o $@ $^

clean:
	-rm $(OUTPUT) $(OBJS)