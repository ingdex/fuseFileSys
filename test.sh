sudo umount tmp
gcc -Wall recovery.c `pkg-config fuse3 --cflags --libs` -o recovery
sudo ./recovery tmp/ -o allow_other -o umask=0000 -d
