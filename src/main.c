#define FUSE_USE_VERSION 26
#include <fuse.h>
#include <string.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include "../include/path_utils.h"

static int ufs_getattr(const char *path, struct stat *st) {
    printf("[GETATTR] %s\n", path);
    char rpath[4096];
    if (resolve_path(rpath, sizeof(rpath), path) < 0)
        return -ENOENT;
    if (lstat(rpath, st) == -1)
        return -errno;
    return 0;
}

static int ufs_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
                       off_t offset, struct fuse_file_info *fi) {
    printf("[READDIR] %s\n", path);
    (void)offset; (void)fi;
    filler(buf, ".", NULL, 0);
    filler(buf, "..", NULL, 0);

    char upath[4096], lpath[4096];
    build_upper_path(upath, sizeof(upath), path);
    build_lower_path(lpath, sizeof(lpath), path);

    DIR *dp;
    struct dirent *de;

    if ((dp = opendir(upath)) != NULL) {
        while ((de = readdir(dp)) != NULL) {
            if (strcmp(de->d_name, ".") == 0 ||
                strcmp(de->d_name, "..") == 0) continue;
            filler(buf, de->d_name, NULL, 0);
        }
        closedir(dp);
    }
    if ((dp = opendir(lpath)) != NULL) {
        while ((de = readdir(dp)) != NULL) {
            if (strcmp(de->d_name, ".") == 0 ||
                strcmp(de->d_name, "..") == 0) continue;
            filler(buf, de->d_name, NULL, 0);
        }
        closedir(dp);
    }
    return 0;
}

static int copy_up(const char *path) {
    char lpath[4096], upath[4096];
    build_lower_path(lpath, sizeof(lpath), path);
    build_upper_path(upath, sizeof(upath), path);

    FILE *src = fopen(lpath, "rb");
    if (!src) return -errno;
    FILE *dst = fopen(upath, "wb");
    if (!dst) { fclose(src); return -errno; }

    char buf[4096];
    size_t n;
    while ((n = fread(buf, 1, sizeof(buf), src)) > 0)
        fwrite(buf, 1, n, dst);

    fclose(src);
    fclose(dst);
    return 0;
}

static int ensure_in_upper(const char *path) {
    char upath[4096], lpath[4096];
    build_upper_path(upath, sizeof(upath), path);
    build_lower_path(lpath, sizeof(lpath), path);
    if (access(upath, F_OK) != 0 && access(lpath, F_OK) == 0)
        return copy_up(path);
    return 0;
}

static int ufs_open(const char *path, struct fuse_file_info *fi) {
    printf("[OPEN] %s\n", path);
    if ((fi->flags & O_ACCMODE) != O_RDONLY)
        ensure_in_upper(path);

    char rpath[4096];
    if (resolve_path(rpath, sizeof(rpath), path) < 0)
        return -ENOENT;
    int fd = open(rpath, fi->flags);
    if (fd == -1) return -errno;
    fi->fh = fd;
    return 0;
}

static int ufs_read(const char *path, char *buf, size_t size,
                    off_t offset, struct fuse_file_info *fi) {
    printf("[READ] %s\n", path);
    (void)path;
    int res = pread(fi->fh, buf, size, offset);
    if (res == -1) return -errno;
    return res;
}

static int ufs_release(const char *path, struct fuse_file_info *fi) {
    (void)path;
    close(fi->fh);
    return 0;
}

static int ufs_write(const char *path, const char *buf, size_t size,
                     off_t offset, struct fuse_file_info *fi) {
    printf("[WRITE] %s\n", path);
    (void)path;
    int res = pwrite(fi->fh, buf, size, offset);
    if (res == -1) return -errno;
    return res;
}

static int ufs_create(const char *path, mode_t mode,
                      struct fuse_file_info *fi) {
    printf("[CREATE] %s\n", path);
    char upath[4096];
    build_upper_path(upath, sizeof(upath), path);
    int fd = open(upath, fi->flags | O_CREAT, mode);
    if (fd == -1) return -errno;
    fi->fh = fd;
    return 0;
}

static int ufs_truncate(const char *path, off_t size) {
    printf("[TRUNCATE] %s\n", path);
    ensure_in_upper(path);
    char upath[4096];
    build_upper_path(upath, sizeof(upath), path);
    if (truncate(upath, size) == -1)
        return -errno;
    return 0;
}

static struct fuse_operations ufs_ops = {
    .getattr  = ufs_getattr,
    .readdir  = ufs_readdir,
    .open     = ufs_open,
    .read     = ufs_read,
    .write    = ufs_write,
    .create   = ufs_create,
    .release  = ufs_release,
    .truncate = ufs_truncate,
};

int main(int argc, char *argv[]) {
    fprintf(stderr, "[unionfs] starting...\n");
    return fuse_main(argc, argv, &ufs_ops, NULL);
}
