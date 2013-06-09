CC = cc
CFLAGS = -Wall -O2 -DTROFFROOT=\"/root/troff/home\"
LDFLAGS =

all: post
%.o: %.c post.h
	$(CC) -c $(CFLAGS) $<
post: post.o out.o ps.o font.o dev.o
	$(CC) -o $@ $^ $(LDFLAGS)
clean:
	rm -f *.o post
