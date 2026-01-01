CC      ?= cc
CFLAGS  ?= -O2 -Wall -Wextra -std=c11
LDFLAGS ?=
LDLIBS  ?= -lncurses -lm

BIN = xmas
SRC = xmas.c

.PHONY: all clean run

all: $(BIN)

$(BIN): $(SRC)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS) $(LDLIBS)

run: $(BIN)
	./$(BIN)

clean:
	rm -f $(BIN)

