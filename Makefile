#
# Makefile for the Loki registry library
# $Id: Makefile,v 1.8 2000-10-21 00:36:47 hercules Exp $
#

CC		= gcc
OBJS	= setupdb.o md5.o arch.o
OS      = $(shell uname -s)

CFLAGS	= -Wall -g -O2 -I. $(shell xml-config --cflags)
LIBS	= $(shell xml-config --libs)

all: libsetupdb.a

libsetupdb.a: $(OBJS) brandelf
	ar rcs $@ $(OBJS)

convert: convert.o libsetupdb.a
	$(CC) -g -o $@ $^ $(LIBS) -static -s
	./brandelf -t $(OS) $@

brandelf: brandelf.o
	$(CC) -o $@ $^

md5sum: md5.c
	$(CC) $(CFLAGS) -o $@ md5.c -DMD5SUM_PROGRAM -lz
	./brandelf -t $(OS) $@

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
