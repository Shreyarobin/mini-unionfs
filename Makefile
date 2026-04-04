CC = gcc
CFLAGS = -Wall

TARGET = unionfs
SRC = main.c

all:
	$(CC) $(CFLAGS) -o $(TARGET) $(SRC)

clean:
	rm -f $(TARGET)