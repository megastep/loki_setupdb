#
# Makefile for the Loki registry library
# $Id: Makefile,v 1.14 2000-11-22 21:58:32 megastep Exp $
#

CC		:= gcc
CSRC	:= setupdb.c md5.c arch.c
OS      := $(shell uname -s)
ARCH    := $(shell sh print_arch)
OBJS    := $(CSRC:%.c=$(ARCH)/%.o)

CFLAGS	= -fsigned-char -Wall -g -O2 -I. $(shell xml-config --cflags)
LIBS	= $(shell xml-config --libs)

IMAGE	= /loki/patch-tools/convert-image

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

install: convert register
	@cp -v convert register $(IMAGE)/bin/$(OS)/$(ARCH)/

brandelf: $(ARCH) $(ARCH)/brandelf.o
	$(CC) -o $@ $(ARCH)/brandelf.o

md5sum: md5.c
	$(CC) $(CFLAGS) -o $@ md5.c -DMD5SUM_PROGRAM -lz -static
	strip $@
	brandelf -t $(OS) $@

register: register.c
	$(CC) $(CFLAGS) -o $@ register.c $(TARGET) $(LIBS) -static
	strip $@
	brandelf -t $(OS) $@

clean:
	rm -f $(ARCH)/*.o *~

distclean: clean
	rm -f $(ARCH)/*.a
	rm -f convert md5sum brandelf register

dep: depend

depend:
	$(CC) -MM $(CFLAGS) $(CSRC) > .depend

ifeq ($(wildcard .depend),.depend)
include .depend
endif
