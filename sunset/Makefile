CC = gcc
CFLAGS = -O2 -Wall
LDFLAGS = -lncurses -lm
TARGET = sunset
SRC = sunset.c

all: $(TARGET)

$(TARGET): $(SRC)
	$(CC) $(CFLAGS) -o $(TARGET) $(SRC) $(LDFLAGS)

clean:
	rm -f $(TARGET)

.PHONY: all clean
