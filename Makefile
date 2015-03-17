TARGETS	= test_ringbuf

test_ringbuf: test_ringbuf.o ringbuf.o

#
# you should define TARGETS in your Makefile so that useful target
# in this file works properly.
#     e.g. TARGETS= hoge2 hoge3
#

.PHONY: clean

OS= $(shell uname -s)

CFLAGS += -Werror -Wall
CFLAGS += -I.

all: $(TARGETS)

$(TARGETS):

#
#.SUFFIXES: .o .c
#
#.c.o:
#	$(CC) $(CFLAGS) -c $<

clean:
	-rm -rf *.dSYM *.o
ifneq ($(TARGETS),)
	-rm -f $(TARGETS)
endif

distclean: clean
	-rm -f Makefile config.cache config.status config.log .depend

make_var_test:
	@echo CFLAGS =$(CFLAGS)
	@echo LDFLAGS=$(LDFLAGS)

