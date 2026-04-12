#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include "../include/path_utils.h"

const char *upper_dir = "upper";
const char *lower_dir = "lower";

void build_upper_path(char *dest, size_t size, const char *path) {
    snprintf(dest, size, "%s%s", upper_dir, path);
}

void build_lower_path(char *dest, size_t size, const char *path) {
    snprintf(dest, size, "%s%s", lower_dir, path);
}

int resolve_path(char *dest, size_t size, const char *path) {
    build_upper_path(dest, size, path);
    if (access(dest, F_OK) == 0) return 0;
    build_lower_path(dest, size, path);
    if (access(dest, F_OK) == 0) return 0;
    return -ENOENT;
}
