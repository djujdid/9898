#include<copy.h>

int block_cur(const char*srcfile,int pronum){
    int fd=open("srcfile",O_RDWR);
	int filesize=lseek(fd,0,SEEK_END);
	if(filesize%pronum==0){
		return filesize/pronum;
		}else{
		return filesize/pronum+1;
			}
	}
