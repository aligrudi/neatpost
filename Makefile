# neatpost's default font directory
FDIR = /neatroff/font

CC = cc
CFLAGS = -Wall -O2 "-DTROFFFDIR=\"$(FDIR)\""
LDFLAGS = -lm
OBJS = post.o ps.o font.o dev.o clr.o dict.o iset.o sbuf.o
OBJSPDF = post.o pdf.o pdfext.o font.o dev.o clr.o dict.o iset.o sbuf.o

all: post pdf
%.o: %.c post.h
	$(CC) -c $(CFLAGS) $<
post: $(OBJS)
	$(CC) -o $@ $(OBJS) $(LDFLAGS)
pdf: $(OBJSPDF)
	$(CC) -o $@ $(OBJSPDF) $(LDFLAGS)
clean:
	rm -f *.o post pdf
