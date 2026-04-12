#include <stdio.h>
#include <errno.h>
#include "../include/cow.h"

int copy_file(const char *src, const char *dest) {
    FILE *source = fopen(src, "rb");
    if (!source) return -1;

    FILE *target = fopen(dest, "wb");
    if (!target) {
        fclose(source);
        return -1;
    }

    char buffer[4096];
    size_t bytes;
    while ((bytes = fread(buffer, 1, sizeof(buffer), source)) > 0)
        fwrite(buffer, 1, bytes, target);

    fclose(source);
    fclose(target);
    return 0;
}
