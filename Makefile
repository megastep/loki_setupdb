#
# Makefile for the Loki registry library
# $Id: Makefile,v 1.10 2000-10-23 21:55:17 hercules Exp $
#

CC		:= gcc
CSRC	:= setupdb.c md5.c arch.c
OS      := $(shell uname -s)
ARCH    := $(shell sh print_arch)
OBJS    := $(CSRC:%.c=$(ARCH)/%.o)

CFLAGS	= -fsigned-char -Wall -g -O2 -I. $(shell xml-config --cflags)
LIBS	= $(shell xml-config --libs)

override TARGET = $(ARCH)/libsetupdb.a

all: $(TARGET)

$(ARCH):
	mkdir $(ARCH)

$(ARCH)/%.o: %.c
	$(CC) -o $@ $(CFLAGS) -c $<

$(TARGET): $(ARCH) $(OBJS)
	ar rcs $@ $(OBJS)

convert: $(ARCH) $(ARCH)/convert.o $(TARGET)
	$(CC) -g -o $@ $(ARCH)/convert.o $(TARGET) $(LIBS) -static
	strip $@
	brandelf -t $(OS) $@

brandelf: $(ARCH) $(ARCH)/brandelf.o
	$(CC) -o $@ $(ARCH)/brandelf.o

md5sum: md5.c
	$(CC) $(CFLAGS) -o $@ md5.c -DMD5SUM_PROGRAM -lz
	strip $@
	brandelf -t $(OS) $@

clean:
	rm -f $(ARCH)/*.o *~

distclean: clean
	rm -f $(ARCH)/*.a
	rm -f convert md5sum brandelf

dep: depend

depend:
	$(CC) -MM $(CFLAGS) $(OBJS:.o=.c) > .depend

ifeq ($(wildcard .depend),.depend)
include .depend
endif
