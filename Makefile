#
# Makefile for the Loki registry library
# $Id: Makefile,v 1.5 2000-10-16 23:13:13 hercules Exp $
#

OBJS	= setupdb.o md5.o

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
