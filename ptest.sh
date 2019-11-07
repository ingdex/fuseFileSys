sudo umount tmp
gcc -Wall passthrough.c `pkg-config fuse3 --cflags --libs` -o passthrough
sudo ./passthrough tmp/ -o allow_other -o umask=0000 -d
