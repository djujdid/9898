#include<copy.h>

int check_pram(int argc,const char*srcfile,int pronum){
       if(argc<3){
		   perror("参数小于3");
		   exit(EXIT_FAILURE);
		   }
	if((access(srcfile,F_OK))!=0){
		perror("文件不存在");
		exit(EXIT_FAILURE);
		}
		if(pronum<1||pronum>=100){
			perror("进程数量错误");
			exit(EXIT_FAILURE);
			}
			return 1;
	}
