#
# Makefile for the Loki registry library
# $Id: Makefile,v 1.3 2000-10-12 02:32:49 megastep Exp $
#

CC		= gcc
OBJS	= setupdb.o md5.o

CFLAGS	= -Wall -g -O2 -I. $(shell xml-config --cflags)
LIBS	= $(shell xml-config --libs)

all: libsetupdb.a convert

libsetupdb.a: $(OBJS)
	ar rcs $@ $(OBJS)

convert: convert.o libsetupdb.a
	$(CC) -g -o $@ $^ $(LIBS)

clean:
	rm -f *.o *.a *~ convert md5sum

md5sum:
	$(CC) $(CFLAGS) -o $@ md5.c -DMD5SUM_PROGRAM -lz
