CC = gcc
CFLAGS = -O2 -Wall -std=c11
LDFLAGS = -lncurses -lm
TARGET = defender
SRC = main.c game.c input.c render.c

all: $(TARGET)

$(TARGET): $(SRC)
	$(CC) $(CFLAGS) -o $(TARGET) $(SRC) $(LDFLAGS)

clean:
	rm -f $(TARGET)

.PHONY: all clean
