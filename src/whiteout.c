#include <stdio.h>
#include <string.h>
#include "../include/whiteout.h"

void create_whiteout(char *dest, const char *path) {
    sprintf(dest, "upper/.wh_%s", path + 1);
}

int is_whiteout(const char *name) {
    return strncmp(name, ".wh_", 4) == 0;
}
