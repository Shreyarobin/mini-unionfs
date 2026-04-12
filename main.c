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

static const char *lower_dir = "lower";
static const char *upper_dir = "upper";

static int resolve_path(const char *path, char *out, size_t size) {
    snprintf(out, size, "%s%s", upper_dir, path);
    if (access(out, F_OK) == 0) return 0;
    snprintf(out, size, "%s%s", lower_dir, path);
    if (access(out, F_OK) == 0) return 0;
    return -ENOENT;
}

static int ufs_getattr(const char *path, struct stat *st) {
    char rpath[4096];
    if (resolve_path(path, rpath, sizeof(rpath)) < 0)
        return -ENOENT;
    if (lstat(rpath, st) == -1)
        return -errno;
    return 0;
}

static int ufs_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
                       off_t offset, struct fuse_file_info *fi) {
    (void)offset; (void)fi;
    filler(buf, ".", NULL, 0);
    filler(buf, "..", NULL, 0);

    char upath[4096], lpath[4096];
    snprintf(upath, sizeof(upath), "%s%s", upper_dir, path);
    snprintf(lpath, sizeof(lpath), "%s%s", lower_dir, path);

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

static int ufs_open(const char *path, struct fuse_file_info *fi) {
    char rpath[4096];
    if (resolve_path(path, rpath, sizeof(rpath)) < 0)
        return -ENOENT;
    int fd = open(rpath, fi->flags);
    if (fd == -1) return -errno;
    fi->fh = fd;
    return 0;
}

static int ufs_read(const char *path, char *buf, size_t size,
                    off_t offset, struct fuse_file_info *fi) {
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
    (void)path;
    int res = pwrite(fi->fh, buf, size, offset);
    if (res == -1) return -errno;
    return res;
}

static int ufs_create(const char *path, mode_t mode,
                      struct fuse_file_info *fi) {
    char upath[4096];
    snprintf(upath, sizeof(upath), "%s%s", upper_dir, path);
    int fd = open(upath, fi->flags | O_CREAT, mode);
    if (fd == -1) return -errno;
    fi->fh = fd;
    return 0;
}

static struct fuse_operations ufs_ops = {
    .getattr = ufs_getattr,
    .readdir = ufs_readdir,
    .open    = ufs_open,
    .read    = ufs_read,
    .write   = ufs_write,
    .create  = ufs_create,
    .release = ufs_release,
};

int main(int argc, char *argv[]) {
    fprintf(stderr, "[unionfs] starting...\n");
    return fuse_main(argc, argv, &ufs_ops, NULL);
}
