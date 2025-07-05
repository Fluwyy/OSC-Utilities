CC = gcc
CFLAGS = -Wall -Wextra -std=c99 -pthread
TARGET = osc_utility
SOURCES = main.c socket.c oscUtility.c cli.c mediaControl.c rateLimiter.c keyPress.c

$(TARGET): $(SOURCES)
	$(CC) $(CFLAGS) -o $(TARGET) $(SOURCES)

clean:
	rm -f $(TARGET)

.PHONY: clean
