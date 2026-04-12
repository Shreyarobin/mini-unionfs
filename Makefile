CC = gcc
CFLAGS = -Wall -D_FILE_OFFSET_BITS=64 `pkg-config fuse --cflags`
LIBS = `pkg-config fuse --libs`

TARGET = unionfs
SRC = main.c

all:
	$(CC) $(CFLAGS) -o $(TARGET) $(SRC) $(LIBS)

clean:
	rm -f $(TARGET)
