/*
  FUSE: Filesystem in Userspace
  Copyright (C) 2001-2007  Miklos Szeredi <miklos@szeredi.hu>
  Copyright (C) 2011       Sebastian Pipping <sebastian@pipping.org>

  This program can be distributed under the terms of the GNU GPL.
  See the file COPYING.
*/

/** @file
 *
 * This file system mirrors the existing file system hierarchy of the
 * system, starting at the root file system. This is implemented by
 * just "passing through" all requests to the corresponding user-space
 * libc functions. Its performance is terrible.
 *
 * Compile with
 *
 *     gcc -Wall passthrough.c `pkg-config fuse3 --cflags --libs` -o passthrough
 *
 * ## Source code ##
 * \include passthrough.c
 */


#define FUSE_USE_VERSION 30

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

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
#include <sys/time.h>
#ifdef HAVE_SETXATTR
#include <sys/xattr.h>
#endif

static void *xmp_init(struct fuse_conn_info *conn,
		      struct fuse_config *cfg)
{
	(void) conn;
	cfg->use_ino = 1;
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

	/* On Linux this could just be 'mknod(path, mode, rdev)' but this
	   is more portable */
	if (S_ISREG(mode)) {
		res = open(path, O_CREAT | O_EXCL | O_WRONLY, mode);
		if (res >= 0)
			res = close(res);
	} else if (S_ISFIFO(mode))
		res = mkfifo(path, mode);
	else
		res = mknod(path, mode, rdev);
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
	(void) fi;
	int res;

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
    //将逻辑偏移转换为物理偏移
    off_t loffset = offset;//逻辑偏移
    offset = offset/4088*4096+offset%4088;
    int fd;
    int res = 0;
    (void) fi;
    fd = open(path, O_RDONLY);
    if (fd == -1)
        return -errno;
    size_t last = 4088-loffset%4088;  //当前文件指针所在块剩余数据大小
    size_t left = size; //剩余需要读取的数据大小
    size_t inoff = 0;   //当前已经读取的文件大小，用于偏移值计算
//    char blk[4096];
    //debug
    printf("\n--------------------\n");
    printf("path:%s\n", path);
    printf("size:%lu\n", size);
    printf("offset:%lu\n", offset);
    printf("loffset:%lu\n", loffset);
    printf("inoff:%lu\n", inoff);
    printf("last:%lu\n", last);
    printf("left:%lu\n", left);
//    printf("%x %x %x %x %x %x %x %x\n", FRM[0], FRM[1], FRM[2], FRM[3], FRM[4], FRM[5], FRM[6], FRM[7]);
    printf("\n--------------------\n");
    //FRM前两位为00b代表普通FRM，
    // 01b代表最后一块有实际数据的FRM，NRM也在此数据块中，
    // 10b代表最后一块有实际数据的FRM。同时还有一块数据块是只有NRM的数据块，
    // 11b代表此为只有NRM的数据块
    if (size <= last)   //需要读取的数据较少，在当前文件指针所在块即可全部读取
    {
        res = pread(fd, buf+res, last+8, offset);//将最后一块全部读出
        if (res == -1)
        {
            res = -errno;
            if(fi == NULL)
                close(fd);
            return res;
        }
        else if (res == 0)
        {
            if(fi == NULL)
                close(fd);
            return res;
        }
        char flag = buf[last];
        if (((flag>>30)&0x3) == 0x3)//读到的是只有NRM的块
        {
            res = 0;
            if(fi == NULL)
                close(fd);
            return res;
        }
        unsigned int off = *(unsigned int *)&buf[last+4];
        res = (off-loffset)<size?(off-loffset):size;
        //debug
        printf("\n--------------------\n");
        printf("1");
        printf("res:%d\n", res);
        printf("buf:%s", buf);
        printf("\n--------------------\n");
    }
    else //需要读取的数据跨越4k块边界
    {
        res = pread(fd, buf+res, last+8, offset);//将最后一块全部读出
        if (res == -1)
        {
            res = -errno;
            if(fi == NULL)
                close(fd);
            return res;
        }
        else if (res == 0)
        {
            if(fi == NULL)
                close(fd);
            return res;
        }
        char flag = buf[last];
        if (((flag>>30)&0x3) == 0x3)//读到的是只有NRM的块
        {
            res = -errno;
            if(fi == NULL)
                close(fd);
            return res;
        }
        unsigned int off = *(unsigned int *)&buf[last+4];
        res = off - loffset;
        inoff = inoff + last + 8;//偏移值计算需要带上FRM
        left -= last;
        //debug
        printf("\n--------------------\n");
        printf("2\n");
        printf("res:%d\n", res);
        printf("inoff:%lu\n", inoff);
        printf("left:%lu\n", left);
        printf("buf:%s", buf);
        printf("\n--------------------\n");
        while (left >= 4088)
        {
            int tmp = pread(fd, buf+res, 4096, offset+inoff);
            if (tmp == -1)
            {
                left = 0;
                break;
            }
            else if (tmp == 0)
            {
                if(fi == NULL)
                    close(fd);
                return res;
            }
            inoff += 4096;
            left -= 4088;
            flag = buf[res+4088];
            off = *(unsigned int *)&buf[res+4088+4];
            printf("\n--------------------\n");
            printf("22\n");
            printf("res:%d\n", res);
            printf("flag:%X\n", (int)flag);
            printf("inoff:%lu\n", inoff);
            printf("left:%lu\n", left);
            printf("flag:%c", flag);
            printf("\n--------------------\n");
            if (((flag>>30)&0x3) == 0x3)//读到的是只有NRM的块
            {
                printf("\n--------------------\n");
                printf("22\n");
                printf("(flag>>30)&0x3) == 0x3");
                printf("\n--------------------\n");
                left = 0;
                break;
            }
            res += (off - res);
            printf("\n--------------------\n");
            printf("3\n");
            printf("res:%d\n", res);
            printf("inoff:%lu\n", inoff);
            printf("left:%lu\n", left);
            printf("buf:%s", buf);
            printf("\n--------------------\n");
        }
        if (left > 0)
        {
            int tmp = pread(fd, buf+res, 4096, offset+inoff);
            if (tmp > 0)
            {
                printf("\n--------------------\n");
                printf("44\n");
                printf("tmp:%d\n", tmp);
                printf("buf:%s", buf);
                printf("\n--------------------\n");
                flag = buf[res+4088];
                off = *(unsigned int *)&buf[res+4088+4];
                if (((flag>>30)&0x3) != 0x3)//读到的不是只有NRM的块
                {
                    res += (off-res)<tmp?(off-res):tmp;
                }
            }

            printf("\n--------------------\n");
            printf("4\n");
            printf("res:%d\n", res);
            printf("tmp:%d\n", tmp);
            printf("inoff:%lu\n", inoff);
            printf("left:%lu\n", left);
            printf("buf:%s", buf);
            printf("\n--------------------\n");
        }
    }
    //计算需要读取的物理文件的大小
    //realSize = （offset+size）大小的文件所需的实际物理空间大小-offset大小的文件所需的实际物理空间大小
//    size_t realSize = (offset+size)/4088*4096+(offset+size)%4088 - offset/4088*4096-offset%4088;
//    char tmp[realSize]; //临时存放带有FRM的数据
//    res = pread(fd, tmp, realSize, offset);
    //将tmp缓冲区中的FRM去除后放入buf


    if (res == -1)
        res = -errno;
    if(fi == NULL)
        close(fd);
    return res;
}
static int xmp_write(const char *path, const char *buf, size_t size,
                     off_t offset, struct fuse_file_info *fi)
{
    //将逻辑偏移转换为物理偏移
    off_t loffset = offset;//逻辑偏移
    offset = offset/4088*4096+offset%4088;
    int fd;
    int res = 0;
    (void) fi;
    char FRM[8];
    fd = open(path, O_WRONLY);
    if (fd == -1)
        return -errno;
    int pathlen = strlen(path)+1;
    char hash[4]={'x','u','z','l'};
    for(int i=0;i<pathlen;i++){
        hash[0] = hash[0]^path[i];
        hash[1] = hash[1]^path[i];
        hash[2] = hash[2]^path[i];
        hash[3] = hash[3]^path[i];
    }
    FRM[0]=hash[0]>>2;  //FRM前两位为00b代表普通FRM，01b代表最后一块有实际数据的FRM，NRM也在此数据块中，10b代表最后一块有实际数据的FRM。同时还有一块数据块是只有NRM的数据块，11b代表此为只有NRM的数据块
    FRM[1]=hash[1];
    FRM[2]=hash[2];
    FRM[3]=hash[3];
    off_t inoff = 0;//已写入物理数据大小，即已写入的文件数据以及其他数据大小之和
    int datasize = 0;//已写入逻辑数据大小，即已写入的文件有效内容的大小
    //debug
    printf("\n--------------------\n");
    printf("path:%s\n", path);
    printf("size:%lu\n", size);
    printf("offset:%lu\n", offset);
    printf("inoff:%lu\n", inoff);
    printf("buf:%s", buf);
//    printf("%x %x %x %x %x %x %x %x\n", FRM[0], FRM[1], FRM[2], FRM[3], FRM[4], FRM[5], FRM[6], FRM[7]);
    printf("\n--------------------\n");
    while( size - res >= 4088 ){//如果还有一整块
        //debug
        printf("\n--------------------\n");
        printf("size - inoff >= 4088\n");
        printf("\n--------------------\n");
        //itoa(offset+inoff,blk,10);//*
        if( inoff== 0 ){
            //debug
            printf("\n--------------------\n");
            printf("inoff == 0\n");
            printf("\n--------------------\n");
            //先把不满一块的块填满
            datasize = pwrite(fd, buf+res, 4088-(loffset%4088), offset+inoff);
            if (datasize == -1)
            {
                res = -errno;
                if(fi == NULL)
                    close(fd);
                return res;
            }
            res += datasize;
            inoff += datasize;
        }
        else{
            //debug
            printf("\n--------------------\n");
            printf("inoff != 0\n");
            printf("\n--------------------\n");
            datasize = pwrite(fd, buf+res, 4088, offset+inoff);
            if (datasize == -1)
            {
                res = -errno;
                if(fi == NULL)
                    close(fd);
                return res;
            }
            res += datasize;
            inoff += datasize;
        }
        *(unsigned int *)&FRM[4] = offset+res; //FRM最后32位模4088可得当前数据块中存储的数据大小
        //debug
        printf("\n--------------------\n");
        printf("FRM[4]:%u\n", *(unsigned int *)&FRM[4]);
        printf("\n--------------------\n");
        datasize = pwrite(fd, FRM, 8 ,offset+inoff);//写入FRM BLK
        if (datasize != 8)
        {
            res = -errno;
            if(fi == NULL)
                close(fd);
            return res;
        }
        inoff += 8;
    }
    datasize = pwrite(fd, buf+res, size-res, offset+inoff);
    res += datasize;
    inoff += datasize;
//    res = pwrite(fd, buf, size, offset);
    //保存路径和路径长度 *
    int length = strlen(path)+1;
    //debug
    printf("\n--------------------\n");
    printf("1\n");
    printf("datasize:%d\n", datasize);
    printf("res:%d\n", res);
    printf("inoff:%lu\n", inoff);
    printf("\n--------------------\n");
    //FRM前两位为00b代表普通FRM，
    // 01b代表最后一块有实际数据的FRM，NRM也在此数据块中，
    // 10b代表最后一块有实际数据的FRM。同时还有一块数据块是只有NRM的数据块，
    // 11b代表此为只有NRM的数据块
    //(offset + inoff)%4096 + sizeof(int) + strlen(path)+1 + 8 = (文件实际物理偏移起始位置+已写实际物理文件大小)%4096 + NRM大小 + FRN大小
    if((offset + inoff)%4096 + sizeof(int) + strlen(path)+1 + 8 <= 4096){//这种情况是指，地址只在一块中的情况
        //结尾块FRM
        if ((offset+inoff)%4096 == 0)   //数据刚好写整数块，即最后一块只存放NRM和FRM
        {
            FRM[0] = 0xC0 | (0x3f & FRM[0]);
        }
        else
        {
            FRM[0] = 0x40 | (0x3f & FRM[0]);
        }
        *(unsigned int *)&FRM[4] = offset+res;
        datasize = pwrite(fd, (char *)&length, sizeof(int), offset+inoff);
        inoff += sizeof(int);
        datasize = pwrite(fd, path, strlen(path)+1, offset+inoff);
        inoff += strlen(path)+1;
        datasize = pwrite(fd, FRM, 8, offset+inoff+4096-(offset+inoff)%4096-8);
        //debug
        printf("\n--------------------\n");
        printf("2\n");
        printf("datasize:%d\n", datasize);
        printf("res:%d\n", res);
        printf("strlen(path)+1:%ld\n", strlen(path)+1);
        printf("inoff:%lu\n", inoff);
        printf("*(unsigned int *)&FRM[4]:%u\n", *(unsigned int *)&FRM[4]);
        printf("addr:%ld\n", offset+inoff+4096-(offset+inoff)%4096-8);
        printf("\n--------------------\n");
    }
    else{//地址在不同的两块中的情况
        //debug
        printf("\n--------------------\n");
        printf("3\n");
        printf("\n--------------------\n");
        //数据结尾块FRM
        FRM[0] = 0x80 | (0x3f & FRM[0]);
        *(unsigned int *)&FRM[4] = offset+res;
        datasize = pwrite(fd, FRM, 8, offset+inoff+4096-(offset+inoff)%4096-8);
        datasize = pwrite(fd, (char *)&length, sizeof(int), ((offset+inoff)/4096+1)*4096);
        inoff += sizeof(int);
        datasize = pwrite(fd, path, strlen(path)+1, ((offset+inoff)/4096+1)*4096+ sizeof(int));
        inoff += strlen(path)+1;
        //结尾块FRM
        FRM[0] = 0xC0 | (0x3f & FRM[0]);
        datasize = pwrite(fd, FRM, 8, ((offset+inoff)/4096+1)*4096+4088);
    }
    //debug
    printf("\n--------------------\n");
    printf("4\n");
    printf("\n--------------------\n");
    if (res == -1)
        res = -errno;
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
	/* Just a stub.	 This method is optional and can safely be left
	   unimplemented */

	(void) path;
	(void) fi;
	return 0;
}

static int xmp_fsync(const char *path, int isdatasync,
		     struct fuse_file_info *fi)
{
	/* Just a stub.	 This method is optional and can safely be left
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

	fd = open(path, O_WRONLY);
	if (fd == -1)
		return -errno;

	res = -posix_fallocate(fd, offset, length);

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

static struct fuse_operations xmp_oper = {
	.init           = xmp_init,
	.getattr	= xmp_getattr,
	.access		= xmp_access,
	.readlink	= xmp_readlink,
	.readdir	= xmp_readdir,
	.mknod		= xmp_mknod,
	.mkdir		= xmp_mkdir,
	.symlink	= xmp_symlink,
	.unlink		= xmp_unlink,
	.rmdir		= xmp_rmdir,
	.rename		= xmp_rename,
	.link		= xmp_link,
	.chmod		= xmp_chmod,
	.chown		= xmp_chown,
	.truncate	= xmp_truncate,
#ifdef HAVE_UTIMENSAT
	.utimens	= xmp_utimens,
#endif
	.open		= xmp_open,
	.read		= xmp_read,
	.write		= xmp_write,
	.statfs		= xmp_statfs,
	.release	= xmp_release,
	.fsync		= xmp_fsync,
#ifdef HAVE_POSIX_FALLOCATE
	.fallocate	= xmp_fallocate,
#endif
#ifdef HAVE_SETXATTR
	.setxattr	= xmp_setxattr,
	.getxattr	= xmp_getxattr,
	.listxattr	= xmp_listxattr,
	.removexattr	= xmp_removexattr,
#endif
};

int main(int argc, char *argv[])
{
	umask(0);
	return fuse_main(argc, argv, &xmp_oper, NULL);
}
