CFLAGS = -Wall -D_FILE_OFFSET_BITS=64 `pkg-config fuse --cflags`
LIBS = `pkg-config fuse --libs`
TARGET = unionfs
SRC = src/main.c src/path_utils.c src/layer_utils.c src/cow.c src/whiteout.c

all:
	$(CC) $(CFLAGS) -I include -o $(TARGET) $(SRC) $(LIBS)

clean:
	rm -f $(TARGET)
