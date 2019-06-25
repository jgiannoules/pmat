.POSIX:
.SUFFIXES:
CC	= cc
CFLAGS	= -Wall -O0 -static
LDLIBS	=
LDFLAGS	= 

all: pmat

pmat: pmat.o
	$(CC) $(LDFLAGS) pmat.o -o pmat $(LDLIBS)

pmat.o: pmat.c pmat.h
	$(CC) $(CFLAGS) -c pmat.c -o pmat.o
	ctags -d pmat.c pmat.h

clean:
	rm -f pmat.o pmat
	rm -f tags
