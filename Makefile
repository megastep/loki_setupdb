#
# Makefile for the Loki registry library
# $Id: Makefile,v 1.1.1.1 2000-10-11 01:59:41 megastep Exp $
#

CC		= gcc
OBJS	= setupdb.o md5.o

CFLAGS	= -Wall -g -O2 -I. $(shell xml-config --cflags)
LIBS	= $(shell xml-config --libs)

libsetupdb.a: $(OBJS)
	ar rcs $@ $(OBJS)

convert: convert.o libsetupdb.a
	$(CC) -g -o $@ $^ $(LIBS)

clean:
	rm -f *.o *.a *~ convert
