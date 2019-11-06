/*
  FUSE: Filesystem in Userspace
  Copyright (C) 2001-2007  Miklos Szeredi <miklos@szeredi.hu>
  Copyright (C) 2011       Sebastian Pipping <sebastian@pipping.org>
  This program can be distributed under the terms of the GNU GPL.
  See the file COPYING.
*/
#define FUSE_USE_VERSION 31
#ifdef HAVE_CONFIG_H
#include <config.h>
#endif
#define _GNU_SOURCE
#ifdef linux
/* For pread()/pwrite()/utimensat() */
#define _XOPEN_SOURCE 700
#endif
#include <fuse.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <dirent.h>
#include <errno.h>
#ifdef __FreeBSD__
#include <sys/socket.h>
#include <sys/un.h>
#endif
#include <sys/time.h>
#ifdef HAVE_SETXATTR
#include <sys/xattr.h>
#endif
#include "passthrough_helpers.h"
static void *xmp_init(struct fuse_conn_info *conn,
                      struct fuse_config *cfg)
{
    (void) conn;
    cfg->use_ino = 1;
    /* Pick up changes from lower filesystem right away. This is
       also necessary for better hardlink support. When the kernel
       calls the unlink() handler, it does not know the inode of
       the to-be-removed entry and can therefore not invalidate
       the cache of the associated inode - resulting in an
       incorrect st_nlink value being reported for any remaining
       hardlinks to this inode. */
    cfg->entry_timeout = 0;
    cfg->attr_timeout = 0;
    cfg->negative_timeout = 0;
    return NULL;
}
static int xmp_getattr(const char *path, struct stat *stbuf,
                       struct fuse_file_info *fi)
{
    (void) fi;
    int res;
    res = lstat(path, stbuf);
    if (res == -1)
        return -errno;
    return 0;
}
static int xmp_access(const char *path, int mask)
{
    int res;
    res = access(path, mask);
    if (res == -1)
        return -errno;
    return 0;
}
static int xmp_readlink(const char *path, char *buf, size_t size)
{
    int res;
    res = readlink(path, buf, size - 1);
    if (res == -1)
        return -errno;
    buf[res] = '\0';
    return 0;
}
static int xmp_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
                       off_t offset, struct fuse_file_info *fi,
                       enum fuse_readdir_flags flags)
{
    DIR *dp;
    struct dirent *de;
    (void) offset;
    (void) fi;
    (void) flags;
    dp = opendir(path);
    if (dp == NULL)
        return -errno;
    while ((de = readdir(dp)) != NULL) {
        struct stat st;
        memset(&st, 0, sizeof(st));
        st.st_ino = de->d_ino;
        st.st_mode = de->d_type << 12;
        if (filler(buf, de->d_name, &st, 0, 0))
            break;
    }
    closedir(dp);
    return 0;
}
static int xmp_mknod(const char *path, mode_t mode, dev_t rdev)
{
    int res;
    res = mknod_wrapper(AT_FDCWD, path, NULL, mode, rdev);
    if (res == -1)
        return -errno;
    return 0;
}
static int xmp_mkdir(const char *path, mode_t mode)
{
    int res;
    res = mkdir(path, mode);
    if (res == -1)
        return -errno;
    return 0;
}
static int xmp_unlink(const char *path)
{
    int res;
    res = unlink(path);
    if (res == -1)
        return -errno;
    return 0;
}
static int xmp_rmdir(const char *path)
{
    int res;
    res = rmdir(path);
    if (res == -1)
        return -errno;
    return 0;
}
static int xmp_symlink(const char *from, const char *to)
{
    int res;
    res = symlink(from, to);
    if (res == -1)
        return -errno;
    return 0;
}
static int xmp_rename(const char *from, const char *to, unsigned int flags)
{
    int res;
    if (flags)
        return -EINVAL;
    res = rename(from, to);
    if (res == -1)
        return -errno;
    return 0;
}
static int xmp_link(const char *from, const char *to)
{
    int res;
    res = link(from, to);
    if (res == -1)
        return -errno;
    return 0;
}
static int xmp_chmod(const char *path, mode_t mode,
                     struct fuse_file_info *fi)
{
    (void) fi;
    int res;
    res = chmod(path, mode);
    if (res == -1)
        return -errno;
    return 0;
}
static int xmp_chown(const char *path, uid_t uid, gid_t gid,
                     struct fuse_file_info *fi)
{
    (void) fi;
    int res;
    res = lchown(path, uid, gid);
    if (res == -1)
        return -errno;
    return 0;
}
static int xmp_truncate(const char *path, off_t size,
                        struct fuse_file_info *fi)
{
    int res;
    if (fi != NULL)
        res = ftruncate(fi->fh, size);
    else
        res = truncate(path, size);
    if (res == -1)
        return -errno;
    return 0;
}
#ifdef HAVE_UTIMENSAT
static int xmp_utimens(const char *path, const struct timespec ts[2],
                       struct fuse_file_info *fi)
{
        (void) fi;
        int res;
        /* don't use utime/utimes since they follow symlinks */
        res = utimensat(0, path, ts, AT_SYMLINK_NOFOLLOW);
        if (res == -1)
                return -errno;
        return 0;
}
#endif
static int xmp_create(const char *path, mode_t mode,
                      struct fuse_file_info *fi)
{
    int res;
    res = open(path, fi->flags, mode);
    if (res == -1)
        return -errno;
    fi->fh = res;
    return 0;
}
static int xmp_open(const char *path, struct fuse_file_info *fi)
{
    int res;
    res = open(path, fi->flags);
    if (res == -1)
        return -errno;
    fi->fh = res;
    return 0;
}
static int xmp_read(const char *path, char *buf, size_t size, off_t offset,
                    struct fuse_file_info *fi)
{
    int fd;
    int res;
    if(fi == NULL)
        fd = open(path, O_RDONLY);
    else
        fd = fi->fh;

    if (fd == -1)
        return -errno;
    res = pread(fd, buf, size, offset);
    if (res == -1)
        res = -errno;
    if(fi == NULL)
        close(fd);
    return res;
}
static int xmp_write(const char *path, const char *buf, size_t size,
                     off_t offset, struct fuse_file_info *fi)
{
    int fd;
    int res;
    (void) fi;
    if(fi == NULL)
        fd = open(path, O_WRONLY);
    else
        fd = fi->fh;
    int pathlen = sizeof(path);
    char hash[4]={'x','u','z','l'};
    for(int i=0;i<pathlen;i++){
        hash[0] = hash[0]^path[i];
        hash[1] = hash[1]^path[i];
        hash[2] = hash[2]^path[i];
        hash[3] = hash[3]^path[i];
    }
    FRM[0]=0x80|(hash[0]>>2);
    FRM[1]=hash[1];
    FRM[2]=hash[2];
    FRM[3]=hash[3];
    char blk[4];    //通过HASH方法确定ID，同时加入10b表示其为FRM
    int inoff=0;
    while( size - inoff >= 4088 ){//如果还有一整块
        //itoa(offset+inoff,blk,10);//*
        if( inoff== 0 ){
            res = pwrite(fd, buf, 4088-(offset%4088), offset+inoff);
            inoff = inoff + 4088-(offset%4088);
        }
        else{
            res = pwrite(fd, buf, 4088, offset+inoff);
            inoff = inoff + 4088;//偏移往下走
        }
        res = pwrite(fd, FRM, 8 ,0);//写入FRM BLK
    }
    res = pwrite(fd, buf, size-offset-inoff, offset+inoff);
    for(int i=5;i<8;i++){
        FRM[i] = 0xff;
    }//偏移值为0xffffffff *
    char NRM[]="";char len[2];
    //itoa(sizeof(path),len,10);                     //*
    strcpy( NRM, path );
    strcat( NRM, len );//保存路径和路径长度 *
    if(size -inoff - offset + sizeof(NRM) + 8 <= 4096){//这种情况是指，地址只在一块中的情况
        res = pwrite(fd, NRM, sizeof(NRM) ,0);
        res = pwrite(fd, FRM, 8 ,0);//结尾块
    }
    else{//地址在不同的两块中的情况
        res = pwrite(fd, NRM, 4096-8-(size-offset-inoff),0);
        //itoa(offset+inoff,blk,10);                    //*
        res = pwrite(fd, FRM, 8 ,0);        //BLK
        res = pwrite(fd, NRM, sizeof(NRM)-4096+8+(size-offset-inoff),4096-8-(size-offset-inoff));
        for(int i=5;i<8;i++){
            FRM[i] = 0xff;
        }
        res = pwrite(fd, FRM, 8 ,0);//结尾块
    }
    if (res == -1)
        res = -errno;
    close(fd);
    return res;
    if(fi == NULL)
        close(fd);
    return res;
}
static int xmp_statfs(const char *path, struct statvfs *stbuf)
{
    int res;
    res = statvfs(path, stbuf);
    if (res == -1)
        return -errno;
    return 0;
}
static int xmp_release(const char *path, struct fuse_file_info *fi)
{
    (void) path;
    close(fi->fh);
    return 0;
}
static int xmp_fsync(const char *path, int isdatasync,
                     struct fuse_file_info *fi)
{
    /* Just a stub.  This method is optional and can safely be left
       unimplemented */
    (void) path;
    (void) isdatasync;
    (void) fi;
    return 0;
}
#ifdef HAVE_POSIX_FALLOCATE
static int xmp_fallocate(const char *path, int mode,
                        off_t offset, off_t length, struct fuse_file_info *fi)
{
        int fd;
        int res;
        (void) fi;
        if (mode)
                return -EOPNOTSUPP;
        if(fi == NULL)
                fd = open(path, O_WRONLY);
        else
                fd = fi->fh;
        
        if (fd == -1)
                return -errno;
        res = -posix_fallocate(fd, offset, length);
        if(fi == NULL)
                close(fd);
        return res;
}
#endif
#ifdef HAVE_SETXATTR
/* xattr operations are optional and can safely be left unimplemented */
static int xmp_setxattr(const char *path, const char *name, const char *value,
                        size_t size, int flags)
{
        int res = lsetxattr(path, name, value, size, flags);
        if (res == -1)
                return -errno;
        return 0;
}
static int xmp_getxattr(const char *path, const char *name, char *value,
                        size_t size)
{
        int res = lgetxattr(path, name, value, size);
        if (res == -1)
                return -errno;
        return res;
}
static int xmp_listxattr(const char *path, char *list, size_t size)
{
        int res = llistxattr(path, list, size);
        if (res == -1)
                return -errno;
        return res;
}
static int xmp_removexattr(const char *path, const char *name)
{
        int res = lremovexattr(path, name);
        if (res == -1)
                return -errno;
        return 0;
}
#endif /* HAVE_SETXATTR */
#ifdef HAVE_COPY_FILE_RANGE
static ssize_t xmp_copy_file_range(const char *path_in,
                                   struct fuse_file_info *fi_in,
                                   off_t offset_in, const char *path_out,
                                   struct fuse_file_info *fi_out,
                                   off_t offset_out, size_t len, int flags)
{
        int fd_in, fd_out;
        ssize_t res;
        if(fi_in == NULL)
                fd_in = open(path_in, O_RDONLY);
        else
                fd_in = fi_in->fh;
        if (fd_in == -1)
                return -errno;
        if(fi_out == NULL)
                fd_out = open(path_out, O_WRONLY);
        else
                fd_out = fi_out->fh;
        if (fd_out == -1) {
                close(fd_in);
                return -errno;
        }
        res = copy_file_range(fd_in, &offset_in, fd_out, &offset_out, len,
                              flags);
        if (res == -1)
                res = -errno;
        close(fd_in);
        close(fd_out);
        return res;
}
#endif
static off_t xmp_lseek(const char *path, off_t off, int whence, struct fuse_file_info *fi)
{
    int fd;
    off_t res;
    if (fi == NULL)
        fd = open(path, O_RDONLY);
    else
        fd = fi->fh;
    if (fd == -1)
        return -errno;
    res = lseek(fd, off, whence);
    if (res == -1)
        res = -errno;
    if (fi == NULL)
        close(fd);
    return res;
}
static struct fuse_operations xmp_oper = {
        .init           = xmp_init,
        .getattr        = xmp_getattr,
        .access         = xmp_access,
        .readlink       = xmp_readlink,
        .readdir        = xmp_readdir,
        .mknod          = xmp_mknod,
        .mkdir          = xmp_mkdir,
        .symlink        = xmp_symlink,
        .unlink         = xmp_unlink,
        .rmdir          = xmp_rmdir,
        .rename         = xmp_rename,
        .link           = xmp_link,
        .chmod          = xmp_chmod,
        .chown          = xmp_chown,
        .truncate       = xmp_truncate,
#ifdef HAVE_UTIMENSAT
        .utimens        = xmp_utimens,
#endif
        .open           = xmp_open,
        .create         = xmp_create,
        .read           = xmp_read,
        .write          = xmp_write,
        .statfs         = xmp_statfs,
        .release        = xmp_release,
        .fsync          = xmp_fsync,
#ifdef HAVE_POSIX_FALLOCATE
        .fallocate      = xmp_fallocate,
#endif
#ifdef HAVE_SETXATTR
.setxattr       = xmp_setxattr,
        .getxattr       = xmp_getxattr,
        .listxattr      = xmp_listxattr,
        .removexattr    = xmp_removexattr,
#endif
#ifdef HAVE_COPY_FILE_RANGE
        .copy_file_range = xmp_copy_file_range,
#endif
        .lseek          = xmp_lseek,
};
int main(int argc, char *argv[])
{
    umask(0);
    return fuse_main(argc, argv, &xmp_oper, NULL);
}