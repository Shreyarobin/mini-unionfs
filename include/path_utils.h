#ifndef PATH_UTILS_H
#define PATH_UTILS_H

extern const char *upper_dir;
extern const char *lower_dir;

void build_upper_path(char *dest, size_t size, const char *path);
void build_lower_path(char *dest, size_t size, const char *path);
int resolve_path(char *dest, size_t size, const char *path);

#endif
