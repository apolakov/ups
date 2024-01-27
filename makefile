# Makefile for server program

# Compiler to use
CC = gcc

# Compiler flags
CFLAGS = -Wall -g

# Object files
OBJS = server.o

# Executable name
TARGET = server

# Default target
all: $(TARGET)

# Link the target with object files
$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^ -lpthread

# Compile the source files into object files
server.o: server.c server.h
	$(CC) $(CFLAGS) -c server.c

# Clean target for cleaning up
clean:
	rm -f $(TARGET) $(OBJS)

# Phony targets
.PHONY: all clean