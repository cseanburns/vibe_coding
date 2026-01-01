CC      ?= cc
CFLAGS  ?= -O2 -Wall -Wextra -std=c11
LDFLAGS ?=
LDLIBS  ?= -lncurses

all: skyline_fireworks

skyline_fireworks: skyline_fireworks.c
	$(CC) $(CFLAGS) -o $@ $< $(LDFLAGS) $(LDLIBS)

clean:
	rm -f skyline_fireworks

.PHONY: all clean
