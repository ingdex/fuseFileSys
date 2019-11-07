bug记录：
在xmp_write函数中，如果使用
if(fi == NULL)
        fd = open(path, O_WRONLY);
    else
        fd = fi->fh;
则实际不会执行open函数，写文件时出现bug：
ingdex@ingdex-System-Product-Name:~/fuseFileSys/tmp$ echo 1>1
bash: echo: 写错误: 非法 seek 操作
更换为
fd = open(path, O_WRONLY);
    if (fd == -1)
        return -errno;
bug消失，具体原因不知
