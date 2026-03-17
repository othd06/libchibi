CFLAGS=-std=c11 -g -fno-common -Wall -Wno-switch

SRCS=$(wildcard *.c)
OBJS=$(SRCS:.c=.o)

# Stage 1

libchibi: $(OBJS)
	ar rcs libchibi.a $(OBJS)

$(OBJS): chibicc.h

# Misc.

clean:
	rm -rf libchibi.a tmp*
	find * -type f '(' -name '*~' -o -name '*.o' ')' -exec rm {} ';'

.PHONY: clean
