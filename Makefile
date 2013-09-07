# neatpost's default font directory
FDIR=/root/troff/home/font

CC = cc
CFLAGS = -Wall -O2 "-DTROFFFDIR=\"$(FDIR)\""
LDFLAGS =

all: post
%.o: %.c post.h
	$(CC) -c $(CFLAGS) $<
post: post.o out.o ps.o font.o dev.o clr.o
	$(CC) -o $@ $^ $(LDFLAGS)
clean:
	rm -f *.o post
