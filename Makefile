#
# Makefile for the Loki registry library
# $Id: Makefile,v 1.6 2000-10-17 03:33:06 megastep Exp $
#

CC		= gcc
OBJS	= setupdb.o md5.o arch.o

CFLAGS	= -Wall -g -O2 -I. $(shell xml-config --cflags)
LIBS	= $(shell xml-config --libs)

all: libsetupdb.a

libsetupdb.a: $(OBJS)
	ar rcs $@ $(OBJS)

convert: convert.o libsetupdb.a
	$(CC) -g -o $@ $^ $(LIBS) -static -s

md5sum:
	$(CC) $(CFLAGS) -o $@ md5.c -DMD5SUM_PROGRAM -lz

clean:
	rm -f *.o *.a *~

distclean: clean
	rm -f convert md5sum

dep: depend

depend:
	$(CC) -MM $(CFLAGS) $(OBJS:.o=.c) > .depend

ifeq ($(wildcard .depend),.depend)
include .depend
endif
