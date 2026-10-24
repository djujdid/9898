#include<copy.h>

int process_create(const char*srcfile,const char*destfile,int pronum,int blocksize){
    pid_t pid;
	int i;
	for(i=0;i<pronum;i++){
		pid=fork();
		if(pid==0){
			int offset=i*blocksize;
			char str_offset[10];
			char str_blocksize[10];
			bzero(str_offset,10);
			bzero(str_blocksize,10);
			sprintf(str_offset,"%d",offset);
			sprintf(str_blocksize,"%d",blocksize);
			execl("/home/colin/colinSoft/20251018/process_copy/Mod/copy","copy",srcfile,destfile,str_offset,str_blocksize,NULL);
             perror("execl failed");
			 exit(EXIT_FAILURE);
			}
			return 0;
		}
   
	}
