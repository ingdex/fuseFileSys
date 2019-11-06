#define FUSE_USE_VERSION 26

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
#include <string.h>
#include "discmanage.h"
#include "rfid.h"
//#include "scsi_fs.h"
#include "operationqueue.h"
#include "fs_message.h"
#include "opstor_error.h"
#ifdef HAVE_SETXATTR
#include <sys/xattr.h>
#endif


static int xmp_getattr(const char *path, struct stat *stbuf)
{

	int res,c_res;
	unsigned int c_mode;
	int c_uid,c_gid;
	unsigned long long c_size;
	FILE *c_file = NULL;
	string filename;
	
	char new_path[NAME_LEN_MAX*2];
	char c_buf[NAME_LEN_MAX];
	sprintf(new_path,"%s%s",AddToCPath,path);

	//if the path points to a folder, we need to do nothing

	if(check_long_name(path) == 0)
		return -36;
	
	if(ispathdir(new_path) == 1)
	{
		res = lstat(new_path, stbuf);
		printf("dir mode:%d\n",stbuf->st_mode);
		if (res == -1)
			return -errno;

	}
	//we need to read the size and id info from the file in the client path
	//and the fill the infomation into the struct stat
	else
	{
		if(is_suffix_html(path))
		{
			filename = GetFileName(path);
			filename = OFFPath + filename;
			res = lstat(filename.c_str(),stbuf);
			printf("html\n");
			return res;
		}
		printf("not html\n");
		res = lstat(new_path, stbuf);
		if (res == -1)
			return -errno;
		c_res = write_node_search(new_path);
		if(c_res == -1)
		{
			c_file = fopen(new_path,"r");
			if(c_file == NULL)
				return -errno;
			if(fgets(c_buf,NAME_LEN_MAX,c_file) != NULL)
			{
				sscanf(c_buf,"%u%d%d%llu",&c_mode,&c_uid,&c_gid,&c_size);
				stbuf->st_mode = c_mode;
				stbuf->st_uid = c_uid;
				stbuf->st_gid = c_gid;
				stbuf->st_size = c_size;
			}
			else
			{
			//there is no data in the file 
			//user inappropriate operation
				return -errno;
			}

			fclose(c_file);
		}
		else
		{
			stbuf->st_mode = write_list[c_res].mode;
			stbuf->st_uid = write_list[c_res].uid;
			stbuf->st_gid = write_list[c_res].gid;
			stbuf->st_size = write_list[c_res].size;
		}
	}
	

	return 0;
}

static int xmp_access(const char *path, int mask)
{
	int res;

	char new_path[NAME_LEN_MAX*2];
	sprintf(new_path,"%s%s",AddToCPath,path);
	
	res = access(new_path, mask);
	if (res == -1)
		return -errno;

	return 0;
}

static int xmp_readlink(const char *path, char *buf, size_t size)
{
	int res;

	char new_path[NAME_LEN_MAX*2];
	sprintf(new_path,"%s%s",AddToCPath,path);
	
	res = readlink(new_path, buf, size - 1);
	if (res == -1)
		return -errno;

	buf[res] = '\0';
	return 0;
}


static int xmp_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
		       off_t offset, struct fuse_file_info *fi)
{
	DIR *dp;
	struct dirent *de;

	(void) offset;
	(void) fi;
	FILE_METADATA c_meta;
	int res;

	char new_path[NAME_LEN_MAX*2];//,new_name[NAME_LEN_MAX],abs_path[NAME_LEN_MAX];
	sprintf(new_path,"%s%s",AddToCPath,path);

/*	dp = opendir(new_path);
	if (dp == NULL)
		return -errno;

	while ((de = readdir(dp)) != NULL) {
		struct stat st;
		memset(&st, 0, sizeof(st));
		st.st_ino = de->d_ino;
		st.st_mode = de->d_type << 12;
		sprintf(new_name,"%s",de->d_name);
		if(de->d_type != DT_DIR)
		{
			printf("readdir :%s\n",de->d_name);
			printf("path:%s\n",path);
			sprintf(abs_path,"%s/%s",new_path,de->d_name);
			res = read_metadata_from_file(abs_path,&c_meta);
			res = is_file_allol(c_meta,de->d_name);
			if(res == 0)
			{
				printf("%s not all online\n",de->d_name);
				sprintf(new_name,"%s.html",de->d_name);
			}
		}
		//sprintf(new_name,"%s.html",de->d_name);
		//check a file, the file may be stored in two or more groups
		//if all the group of a file is online then the file is ok to read
		//if not ,then we need to change the suffix of the file to html
		if (filler(buf, new_name, &st, 0))
			break;
	}

	closedir(dp);*/
	

	dp = opendir(new_path);
	if (dp == NULL)
		return -errno;

	while ((de = readdir(dp)) != NULL) {
		struct stat st;
		memset(&st, 0, sizeof(st));
		st.st_ino = de->d_ino;
		st.st_mode = de->d_type << 12;
		if (filler(buf, de->d_name, &st, 0))
			break;
	}

	closedir(dp);
	return 0;
}

static int xmp_mknod(const char *path, mode_t mode, dev_t rdev)
{
	int res,s_res;

	/* On Linux this could just be 'mknod(path, mode, rdev)' but this
	   is more portable */

	char new_path[NAME_LEN_MAX*2],server_path[NAME_LEN_MAX * 2];
	string filename;
	FILE_METADATA c_metadata;
	sprintf(new_path,"%s%s",AddToCPath,path);
	
	if (S_ISREG(mode)) {
		res = open(new_path, O_CREAT | O_EXCL | O_WRONLY, mode);
		if (res >= 0)
			res = close(res);
	} else if (S_ISFIFO(mode))
		res = mkfifo(path, mode);
	else
		res = mknod(new_path, mode, rdev);
	if (res == -1)
		return -errno;

	// get the filename and create the same file in the current server folder
	/*filename = GetFileName(path);

	filename = "/" + filename;
	filename = f_volname + filename;
	filename = "/" + filename;
	filename = AddToSPath + filename;

	s_res = mknod(filename.c_str(),mode,rdev);
	if(s_res == -1)
		return -errno;*/

	//create the metadata structure
	//c_metadata = (FILE_METADATA *)malloc(sizeof(FILE_METADATA));
	//if(c_metadata == NULL)
		//return -errno;
	c_metadata.fd = -1;
	c_metadata.file_path = new_path;
	c_metadata.gid = getgid();
	c_metadata.uid = getuid();
	c_metadata.mode = mode;
	c_metadata.size = 0;
	c_metadata.split_num = 0;

	s_res = save_metadata_to_file(c_metadata);
	if(s_res == -1)
			return -errno;

	return 0;
}

static int xmp_mkdir(const char *path, mode_t mode)
{
	int res;

	char new_path[NAME_LEN_MAX*2];
	sprintf(new_path,"%s%s",AddToCPath,path);

	res = mkdir(new_path, mode);
	if (res == -1)
		return -errno;

	return 0;
}

static int xmp_unlink(const char *path)
{
	int res;

	char new_path[NAME_LEN_MAX*2];
	sprintf(new_path,"%s%s",AddToCPath,path);

	res = unlink(new_path);
	if (res == -1)
		return -errno;

	return 0;
}

static int xmp_rmdir(const char *path)
{
	int res;

	char new_path[NAME_LEN_MAX*2];
	sprintf(new_path,"%s%s",AddToCPath,path);

	res = rmdir(new_path);
	if (res == -1)
		return -errno;

	return 0;
}

static int xmp_symlink(const char *from, const char *to)
{
	int res;

	char new_from[NAME_LEN_MAX*2],new_to[NAME_LEN_MAX * 2];
	sprintf(new_from,"%s%s",AddToCPath,from);

	sprintf(new_to,"%s%s",AddToCPath,to);

	res = symlink(new_from, new_to);
	if (res == -1)
		return -errno;

	return 0;
}


//get the structure and check if the file can be renamed
static int xmp_rename(const char *from, const char *to)
{
	int res;

	char new_from[NAME_LEN_MAX*2],new_to[NAME_LEN_MAX * 2];
	sprintf(new_from,"%s%s",AddToCPath,from);

	sprintf(new_to,"%s%s",AddToCPath,to);

	res = rename(new_from, new_to);
	if (res == -1)
		return -errno;

	return 0;
}

static int xmp_link(const char *from, const char *to)
{
	int res;

	char new_from[NAME_LEN_MAX*2],new_to[NAME_LEN_MAX * 2];
	sprintf(new_from,"%s%s",AddToCPath,from);

	sprintf(new_to,"%s%s",AddToCPath,to);


	res = link(new_from, new_to);
	if (res == -1)
		return -errno;

	return 0;
}

static int xmp_chmod(const char *path, mode_t mode)
{
	int res;

	char new_path[NAME_LEN_MAX*2];
	sprintf(new_path,"%s%s",AddToCPath,path);

	res = chmod(new_path, mode);
	if (res == -1)
		return -errno;

	return 0;
}

static int xmp_chown(const char *path, uid_t uid, gid_t gid)
{
	int res;

	char new_path[NAME_LEN_MAX*2];
	sprintf(new_path,"%s%s",AddToCPath,path);

	res = lchown(new_path, uid, gid);
	if (res == -1)
		return -errno;

	return 0;
}

static int xmp_truncate(const char *path, off_t size)
{
	int res = 0;

	char new_path[NAME_LEN_MAX*2];
	sprintf(new_path,"%s%s",AddToCPath,path);

	//res = truncate(new_path, size);
	if (res == -1)
		return -errno;

	return 0;
}

#ifdef HAVE_UTIMENSAT
static int xmp_utimens(const char *path, const struct timespec ts[2])
{
	int res;

	char new_path[NAME_LEN_MAX*2];
	sprintf(new_path,"%s%s",AddToCPath,path);

	/* don't use utime/utimes since they follow symlinks */
	res = utimensat(0, new_path, ts, AT_SYMLINK_NOFOLLOW);
	if (res == -1)
		return -errno;

	return 0;
}
#endif

static int xmp_open(const char *path, struct fuse_file_info *fi)
{
	int res;

	char new_path[NAME_LEN_MAX*2];
	sprintf(new_path,"%s%s",AddToCPath,path);
	string filename;

	if(is_suffix_html(path))
	{
		filename = GetFileName(path);
		filename = OFFPath + filename;
		res = open(filename.c_str(), fi->flags);

		if(res == -1)
			return -errno;
		close(res);
		return 0;
	}

	res = open(new_path, fi->flags);
	if (res == -1)
		return -errno;

	close(res);
	return 0;
}

static int xmp_read(const char *path, char *buf, size_t size, off_t offset,
		    struct fuse_file_info *fi)
{
	int fd;
	int res,c_res;
	int loop_time = 0;
	struct stat_msg sta_msg;
	int DiscNo,msg_ret,drive_id = -1;
	string read_disc_volname,volpath,PurePath,filename;
	FILE_METADATA c_metadata;
	FILE_DIS c_dis;
	SDisc DiscOpen;
	off_t new_offset;
	read_disc r_disc,e_disc;
	struct timeval tv;
	
	char new_path[NAME_LEN_MAX*2],mount_s[NAME_LEN_MAX * 2],umount_s[NAME_LEN_MAX * 2],msg_s[NAME_LEN_MAX],fix_s[NAME_LEN_MAX];
	sprintf(new_path,"%s%s",AddToCPath,path);

	if(is_suffix_html(path))
	{
		filename = GetFileName(path);
		filename = OFFPath + filename;
		fd = open(filename.c_str(), O_RDONLY);

		if(fd == -1)
			return -errno;

		res = pread(fd, buf, size, offset);
		if (res == -1)
			res = -errno;
		printf("html\n");
		close(fd);
		return res;
	}

	c_res = read_node_search(new_path);
	printf("location:%d\n",c_res);
	if(c_res == -1)
	{
		res = read_metadata_from_file(new_path,&c_metadata);
		printf("read 1\n");
		if(res == -1)
			return -errno;

		read_list.push_back(c_metadata);
		printf("read 2\n");

		c_res = read_node_search(new_path);
		if(c_res == -1)
			return -errno;
	}

	
	printf("location:%d,%d\n",c_res, read_list[c_res].split_num);
	read_disc_volname = "";
	for(int i = 1; i <= read_list[c_res].split_num; i++)
	{
		printf("start_add:%llu end:%llu\n",c_dis.start_add,c_dis.end_add);
		c_dis = read_list[c_res].file_distributions[i-1];
		if(offset >= c_dis.start_add && offset < c_dis.end_add)
		{
			read_disc_volname = c_dis.volname;
			break;
		}
	}
	printf("find volname:%s\n",read_disc_volname.c_str());
	if(read_disc_volname == "")
	{
		return 0;
	}
	printf("read 3\n");

	volpath = "server_" + read_disc_volname;
	PurePath = "server/" + read_disc_volname;

	refresh_r_time(volpath);
	
	memset(&DiscOpen,0,sizeof(DiscOpen));
	res = GetDiscInfo(volpath,&DiscOpen);

	if(res != -1)//if in the map		
	{
		printf("on map\n");
		if(!IsCached(PurePath))//if chached read ahead, if not
		{
			printf("not cached\n");
			if(!IsOnDrive(volpath))//if ondirve now read ahead,if not
			{
				printf("not on drive\n");
				if(IsOnLibrary(DiscOpen.Disc_group_RFID))//if on the lib,get it and mount,if not
				{
					//onlibrary,we need to get the disc online,then mount

					gettimeofday(&tv,NULL);
					r_disc.volname = volpath;
					r_disc.r_time = tv.tv_sec;
					OnDriveDisc.push_back(r_disc);
					SaveDiscInfo();
					if(OnDriveDisc.size() > maxreaddriveno)
					{
						e_disc = OnDriveDisc.front();
						for(int i = 0;i < drive_no; i++)
						{
							if(p_mutexinfo.CDDriveInfo[i] == USED &&GetBLKID(i) == e_disc.volname)
							{
								//check the time interval 
								if(tv.tv_sec - e_disc.r_time < r_limit_time)
									return -errno;
								//send msg to rm disc
								sprintf(umount_s,"umount /dev/sr%d",i);
								system(umount_s);
								sprintf(msg_s,"read send_rm_cd_sig:%d\t%d\t%d",GetLocByRFID(DiscArray[e_disc.volname].Disc_group_RFID),DiscArray[e_disc.volname].Disc_NO,i);
								sendmsglog(msg_s);
								send_rm_cd_sig(GetLocByRFID(DiscArray[e_disc.volname].Disc_group_RFID),DiscArray[e_disc.volname].Disc_NO,i);
								eject_info[i].status = 1;
								OnDriveDisc.pop_front();
								SaveDiscInfo();
								break;
							}
						}
					}
					//if need to rm disc ,wait 10s for the free drive
					//if no free drive then just return error
					while(drive_id == -1)
					{
						drive_id = GetAVADriveNO();
						if(drive_id != -1)
						{
							pthread_mutex_lock(&p_mutexinfo.lock);
							p_mutexinfo.CDDriveInfo[drive_id] = USED;
							pthread_mutex_unlock(&p_mutexinfo.lock);
							break;
						}
						usleep(500000);
						if(loop_time >= 20)
						{
							sendmsglog("loop to rm disc failed 20 times,cannot get the drive ready");
							return -errno;
						}
						loop_time ++;
					}
					sprintf(msg_s,"send_read_sig:%d\t%d\t%d\n",GetLocByRFID(DiscOpen.Disc_group_RFID),DiscOpen.Disc_NO,drive_id);
					sendmsglog(msg_s);
					send_read_sig(GetLocByRFID(DiscOpen.Disc_group_RFID),DiscOpen.Disc_NO,drive_id);
					loop_time = 0;
					//wait for the load finish msg 
					//loop wait for 30 seconds,60 times each 0.5s
					while(1)
					{
						if(jugmsg(drive_readmsg_receive))
						{
							sta_msg = getfrmstat(drive_readmsg_receive);
							if(strcmp(sta_msg.re_info.operate,"load finish") == 0)
							{
								if(GetBLKID(drive_id) == volpath)
								{
									sprintf(fix_s,"/home/zwj/fix.sh sr%d",drive_id);
									system(fix_s);
									
									sprintf(mount_s,"mount /dev/sr%d /mnt/data/%s",drive_id,PurePath.c_str());
									//printf("mount string:%s\n",mount_s);
									system(mount_s);

									system(fix_s);
								}
								else
								{
									//put in the wrong disc,check the rfid
									//do some after work to solve this
									sendmsglog("put in the wrong disc");     
								}
							}
							else
							{
								switch(sta_msg.error_no)
								{
									case ARM_FINGER_DISK_LOST_ERROR:
									case ARM_DISK_NOT_IN_SLOT_ERROR:
									{
										//can not put the disc into the drive,readd the task to the burn queue
										
										pthread_mutex_lock(&p_mutexinfo.lock);
										p_mutexinfo.CDDriveInfo[drive_id] = EMPTY;
										pthread_mutex_unlock(&p_mutexinfo.lock);
										return -errno;
										break;
									}
									case ARM_DRIVE_OUT_FAILED_ERROR:
									{
										//change the drive's status to busy ,readd the task to the burn queue
										pthread_mutex_lock(&p_mutexinfo.lock);
										p_mutexinfo.CDDriveInfo[drive_id] = ISBUSY;
										pthread_mutex_unlock(&p_mutexinfo.lock);
										return -errno;
										break;
									}
									default:
									{
										return -errno;
										//this error is not proceeded,print it to the log
									}
								}
							}
							break;
						}

						usleep(500000);
						if(loop_time >= 60)
						{
							sendmsglog("loop to waiting for disc 60 times,disc is not in the drive");
							return -errno;
						}

						loop_time ++;
					}
					printf("ONDRIVE now,mount over\n");
				}
			}
		}
	}

	printf("read 4\n");
	printf("offset:%ld start add:%Lu size:%ld\n",offset,c_dis.start_add,size);
	new_offset = offset - c_dis.start_add;

	//filename = GetFileName(path);
	filename = path;
	//filename = "/" + filename;
	filename = read_disc_volname + filename;
	filename = "/" + filename;
	filename = AddToSPath + filename;
	printf("read 5:%s\n",filename.c_str());

	fd = open(filename.c_str(),O_RDONLY);
	if(fd == -1)
		return -errno;

	res = pread(fd, buf, size, new_offset);
	if (res == -1)
		res = -errno;

	close(fd);

	return res;
}

static int xmp_write(const char *path, const char *buf, size_t size,
		     off_t offset, struct fuse_file_info *fi)
{
	int fd;
	int res;
	int c_res;
	FILE_METADATA c_metadata;
	FILE_DIS c_dis;
	FILE_DIS dis_buf;
	FILE_METADATA c_buf;
	string filename,fullpath;
	string write_disc_volname,mkdir_s,cache_dir;
	FILE * c_file = NULL;

	char new_path[NAME_LEN_MAX*2];
	sprintf(new_path,"%s%s",AddToCPath,path);


	//find the path node in the write node list
	c_res = write_node_search(new_path);
	
	//if not in the write node list ,then it is the first write of the file
	//we need to read the info and strcuture from the client file
	if(c_res == -1)
	{
		res = read_metadata_from_file(new_path,&c_metadata);
		printf("write 1\n");
		if(res == -1)
		{
			printf("write failed 1\n");
			return -errno;
		}

		c_metadata.split_num = 1;

		//c_dis = (FILE_DIS*)malloc(sizeof(FILE_DIS));
		printf("write 2\n");
		c_dis.volname = f_volname;
		c_dis.start_add = 0;
		printf("write 3\n");
		c_dis.end_add = 0;
		fullpath = GetFullDir(path);
		fullpath = f_volname + fullpath;
		fullpath = "/" + fullpath;
		fullpath = AddToSPath + fullpath;
		mkcasdir(fullpath);
		

		c_metadata.file_distributions.push_back(c_dis);

		write_list.push_back(c_metadata);

		c_res = write_node_search(new_path);
		if(c_res == -1)
			return -errno;
	}



	//get the split no struct
	printf("write 6\n");

	filename = GetFileName(path);
	fullpath = GetFullDir(path);
	printf("write 7\n");
	filename = "/" + filename;
	fullpath = write_list[c_res].file_distributions[write_list[c_res].split_num - 1].volname + fullpath;
	fullpath = "/" + fullpath;
	fullpath = AddToSPath + fullpath;
	printf("write 8,fullpath %s\n",fullpath.c_str());
	//mkcasdir(fullpath);
	//filename = "/" + filename;
	filename = fullpath + filename;
	printf("filename in disc:%s\n",filename.c_str());

	fd = open(filename.c_str(),O_CREAT |O_WRONLY);
	if(fd == -1)
	{
		printf("write failed 5\n");
		return -errno;
	}
	
	res = pwrite(fd,buf,size,offset - write_list[c_res].file_distributions[write_list[c_res].split_num - 1].start_add);
	printf("write 9\n");
	if(res == -1)
	{
		printf("write failed 6\n");
		return -errno;
	}

	write_list[c_res].size += size;
	write_list[c_res].file_distributions[write_list[c_res].split_num - 1].end_add = write_list[c_res].size;
	printf("write 10\n");
	pthread_mutex_lock(&p_mutexinfo.lock);
	f_size += size;
	if(f_size >= m_size)
	{
		cache_dir = "server/" + f_volname;
		CachedDisc.push_back(cache_dir);
		p_mutexinfo.BurnTaskQueue.push_back(f_volname);

		f_volname = GetPVolName();
		fullpath = GetFullDir(path);
		fullpath = f_volname + fullpath;
		fullpath = "/" + fullpath;
		fullpath = AddToSPath + fullpath;
		mkcasdir(fullpath);
		f_size = 0;
		mkdir_s = "/" + f_volname;
		mkdir_s = AddToSPath + mkdir_s;
		mkdir(mkdir_s.c_str(),0777);
		//dis_buf->end_add = c_metadata->size;
		write_list[c_res].split_num ++;
		dis_buf.start_add = write_list[c_res].size;
		dis_buf.end_add = 0;
		dis_buf.volname = f_volname;
		write_list[c_res].file_distributions.push_back(dis_buf);
		//file_dis_data_insert(c_metadata,f_volname.c_str(),c_metadata->size,0);
	}

	//if(p_mutexinfo.BurnTaskQueue.size() >= (maxcachedno - p_mutexinfo.burnningno))
	if(CachedDisc.size() >= maxcachedno - 1)
	{
		usleep(300000);
		printf("sleep 0.3s\n");
	}
	pthread_mutex_unlock(&p_mutexinfo.lock);
	close(fd);
	return res;
}

static int xmp_statfs(const char *path, struct statvfs *stbuf)
{
	int res;

	char new_path[NAME_LEN_MAX*2];
	sprintf(new_path,"%s%s",AddToCPath,path);

	res = statvfs(new_path, stbuf);
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
	//FILE *c_file = NULL;
	int res;
	int c_res;
	int flags = fi->flags;

	if((flags & O_WRONLY) || (flags & O_RDWR) || (flags & O_CREAT) || (flags & O_EXCL) || (flags & O_TRUNC) || (flags & O_APPEND))
	{
		
	}
    else
    {
       	return 0;
    }

	res = filecopylog(path);
	if(res == -1)
	{
		//log write failed
	}

	char new_path[NAME_LEN_MAX*2];
	sprintf(new_path,"%s%s",AddToCPath,path);

	//save the info to database
//	add_db_record(path);	

	//save the data in client file
	c_res = write_node_search(new_path);

	if(c_res == -1)
		return -1;
	res = save_metadata_to_file(write_list[c_res]);

	//write_list.erase(write_list.begin() + c_res);
	//write_list.shrink_to_fit();
	if(res == -1)
		return -errno;
	
	return res;	
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

	char new_path[NAME_LEN_MAX*2];
	sprintf(new_path,"%s%s",AddToCPath,path);

	(void) fi;

	if (mode)
		return -EOPNOTSUPP;

	fd = open(new_path, O_WRONLY);
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

	char new_path[NAME_LEN_MAX*2];
	sprintf(new_path,"%s%s",AddToCPath,path);
	
	int res = lsetxattr(new_path, name, value, size, flags);
	if (res == -1)
		return -errno;
	return 0;
}

static int xmp_getxattr(const char *path, const char *name, char *value,
			size_t size)
{
	char new_path[NAME_LEN_MAX*2];
	sprintf(new_path,"%s%s",AddToCPath,path);

	int res = lgetxattr(new_path, name, value, size);
	if (res == -1)
		return -errno;
	return res;
}

static int xmp_listxattr(const char *path, char *list, size_t size)
{
		char new_path[NAME_LEN_MAX*2];
	sprintf(new_path,"%s%s",AddToCPath,path);
	
	int res = llistxattr(new_path, list, size);
	if (res == -1)
		return -errno;
	return res;
}

static int xmp_removexattr(const char *path, const char *name)
{
		char new_path[NAME_LEN_MAX*2];
	sprintf(new_path,"%s%s",AddToCPath,path);
	
	int res = lremovexattr(new_path, name);
	if (res == -1)
		return -errno;
	return 0;
}
#endif /* HAVE_SETXATTR */

static struct xmp_operations:fuse_operations 
{
	xmp_operations()
	{
		getattr	= xmp_getattr;
		access	= xmp_access;
		readlink = xmp_readlink;
		readdir	= xmp_readdir;
		mknod	= xmp_mknod;
		mkdir	= xmp_mkdir;
		symlink	= xmp_symlink;
		unlink	= xmp_unlink;
		rmdir	= xmp_rmdir;
		rename	= xmp_rename;
		link	= xmp_link;
		chmod	= xmp_chmod;
		chown	= xmp_chown;
		truncate	= xmp_truncate;
#ifdef HAVE_UTIMENSAT
		utimens	= xmp_utimens;
#endif	
		open = xmp_open;
		read = xmp_read;
		write = xmp_write;
		statfs	= xmp_statfs;
		release	= xmp_release;
		fsync = xmp_fsync;
#ifdef HAVE_POSIX_FALLOCATE	
		fallocate = xmp_fallocate;
#endif
#ifdef HAVE_SETXATTR	
		setxattr	= xmp_setxattr;
		getxattr	= xmp_getxattr;
		listxattr	= xmp_listxattr;
		removexattr	= xmp_removexattr;
#endif
	}
}xmp_oper;


int main(int argc, char *argv[])
{
	int res;
	int ava_no;
	pthread_t burn_thread_t;
	drive_no = getdriveno();
	maxwritedriveno = MAXPBURN;
	maxreaddriveno = drive_no - MAXPBURN;
	capthredhold = CapcityWarning;
	real_group_count = ONLIBGROUPNO;
	maxcachedno = MAXCACHENO;
	m_size = MAXCAPCITY;
	//m_size = m_size * CAPSTEP;
	m_size = m_size * CAPSTEP * CAPSTEP * CAPSTEP;
	f_volname = "first";
	f_size = 0;
	string mkdir_s = f_volname;
	umask(0);

	pthread_mutex_init(&p_mutexinfo.lock,NULL);
	
	p_mutexinfo.burnningno = 0;

	load_index();
	init_msgid();
	GetRFID();
	pthread_create(&burn_thread_t,NULL,burn_thread,NULL);
	InitCDDriveInfo();
/*	while(rfid_init_flag!=1)
	{
		sleep(1);
	}*/
	if((res = LoadDiscInfo()) == -1)
	{//record the last time maxburn no ,and see if it is changed
		//f_volname = GetPVolName();
		//VolName = prefix_s + VolName;
		ava_no = get_ava_discno();
		printf("ava_no:%d\n",ava_no);
		UsedGroupNo = ava_no / group_disc_no;
		UsedDiscNo = ava_no % group_disc_no;
		mkdir_s= "/" + mkdir_s;
		mkdir_s = AddToSPath + mkdir_s;
		printf("first use\n");
		mkdir(mkdir_s.c_str(),0777);
	}
	EjectDisc();
	recover_burning_task();
	RecoverBurnQ();
	printf("used group no :%d\t disc no %d\n",UsedGroupNo,UsedDiscNo);
	res =  fuse_main(argc, argv, &xmp_oper, NULL);
	save_index();
	SaveDiscInfo();
	return res;
}
