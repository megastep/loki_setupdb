#
# Makefile for the Loki registry library
# $Id: Makefile,v 1.4 2000-10-16 21:56:22 megastep Exp $
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

dep: depend

depend:
	$(CC) -MM $(CFLAGS) $(OBJS:.o=.c) > .depend

ifeq ($(wildcard .depend),.depend)
include .depend
endif
