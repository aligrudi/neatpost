# neatpost's default font directory
FDIR = /neatroff/font

CC = cc
CFLAGS = -Wall -O2 "-DTROFFFDIR=\"$(FDIR)\""
LDFLAGS =
OBJS = post.o out.o ps.o font.o dev.o clr.o dict.o iset.o

all: post
%.o: %.c post.h
	$(CC) -c $(CFLAGS) $<
post: $(OBJS)
	$(CC) -o $@ $(OBJS) $(LDFLAGS)
clean:
	rm -f *.o post
