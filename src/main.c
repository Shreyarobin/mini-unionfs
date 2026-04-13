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
#include "../include/layer_utils.h"
#include "../include/cow.h"
#include "../include/whiteout.h"

static int ufs_getattr(const char *path, struct stat *st) {
    char upath[4096], lpath[4096], wh_path[4096];
    build_upper_path(upath, sizeof(upath), path);
    build_lower_path(lpath, sizeof(lpath), path);
    snprintf(wh_path, sizeof(wh_path), "upper/.wh_%s", path + 1);

    if (file_exists(wh_path)) return -ENOENT;

    printf("[MERGE] checking upper for %s\n", path);
    if (file_exists(upath)) {
        if (lstat(upath, st) == -1) return -errno;
        return 0;
    }
    printf("[MERGE] fallback to lower\n");
    if (lstat(lpath, st) == -1) return -ENOENT;
    return 0;
}

static int ufs_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
                       off_t offset, struct fuse_file_info *fi) {
    (void)offset; (void)fi;
    printf("[READDIR] %s\n", path);

    char upath[4096], lpath[4096];
    build_upper_path(upath, sizeof(upath), path);
    build_lower_path(lpath, sizeof(lpath), path);

    DIR *dp;
    struct dirent *de;

    // Read upper, skip whiteout files
    dp = opendir(upath);
    if (dp != NULL) {
        while ((de = readdir(dp)) != NULL) {
            if (is_whiteout(de->d_name)) continue;
            filler(buf, de->d_name, NULL, 0);
        }
        closedir(dp);
    }

    // Read lower, skip duplicates and whiteout-deleted files
    dp = opendir(lpath);
    if (dp != NULL) {
        while ((de = readdir(dp)) != NULL) {
            char temp[4096], wh_path[4096];
            snprintf(temp, sizeof(temp), "%s/%s", upath, de->d_name);
            snprintf(wh_path, sizeof(wh_path), "upper/.wh_%s", de->d_name);
            if (file_exists(temp)) continue;
            if (file_exists(wh_path)) continue;
            filler(buf, de->d_name, NULL, 0);
        }
        closedir(dp);
    }
    return 0;
}

static int ufs_open(const char *path, struct fuse_file_info *fi) {
    printf("[OPEN] %s\n", path);
    char upath[4096], lpath[4096], wh_path[4096];
    build_upper_path(upath, sizeof(upath), path);
    build_lower_path(lpath, sizeof(lpath), path);
    snprintf(wh_path, sizeof(wh_path), "upper/.wh_%s", path + 1);

    if (file_exists(wh_path)) return -ENOENT;

    if ((fi->flags & O_ACCMODE) != O_RDONLY) {
        if (!file_exists(upath) && file_exists(lpath)) {
            printf("[COW] Triggered for %s\n", path);
            if (copy_file(lpath, upath) != 0) return -errno;
        }
    }

    char rpath[4096];
    if (file_exists(upath)) snprintf(rpath, sizeof(rpath), "%s", upath);
    else if (file_exists(lpath)) snprintf(rpath, sizeof(rpath), "%s", lpath);
    else return -ENOENT;

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
    char upath[4096], lpath[4096];
    build_upper_path(upath, sizeof(upath), path);
    build_lower_path(lpath, sizeof(lpath), path);

    if (!file_exists(upath) && file_exists(lpath)) {
        printf("[COW] Triggered for %s\n", path);
        if (copy_file(lpath, upath) != 0) return -errno;
    }

    if (truncate(upath, size) == -1) return -errno;
    return 0;
}

static int ufs_unlink(const char *path) {
    printf("[UNLINK] %s\n", path);
    char upath[4096], lpath[4096], wh_path[4096];
    build_upper_path(upath, sizeof(upath), path);
    build_lower_path(lpath, sizeof(lpath), path);
    snprintf(wh_path, sizeof(wh_path), "upper/.wh_%s", path + 1);

    // File in upper → delete directly
    if (file_exists(upath)) {
        if (unlink(upath) == -1) return -errno;
        return 0;
    }

    // File only in lower → create whiteout
    if (file_exists(lpath)) {
        printf("[WHITEOUT] Created for %s\n", path);
        FILE *f = fopen(wh_path, "w");
        if (!f) return -errno;
        fclose(f);
        return 0;
    }

    return -ENOENT;
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
    .unlink   = ufs_unlink,
};

int main(int argc, char *argv[]) {
    fprintf(stderr, "[unionfs] starting...\n");
    return fuse_main(argc, argv, &ufs_ops, NULL);
}
