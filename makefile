CC=gcc
CFLAGS=-Wall -Wextra -std=c99 -pthread
TARGET=osc_utility
SOURCES=main.c socket.c oscUtility.c cli.c mediaControl.c

all: $(TARGET)

$(TARGET): $(SOURCES)
	$(CC) $(CFLAGS) -o $(TARGET) $(SOURCES)

clean:
	rm -f $(TARGET)

install: $(TARGET)
	cp $(TARGET) /usr/local/bin/

.PHONY: all clean install
