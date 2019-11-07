#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <dirent.h>
#include <errno.h>
#include <sys/time.h>
int main(void)
{
    int fd;
    int res;
    char buf[4096];
    fd = open("./2", O_RDONLY);
    if (fd == -1)
        return -errno;
	res = pread(fd, buf, 4088, 0);
printf("res:%d\nbuf:%s\n", res, buf);
return 0;
}
