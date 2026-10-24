#include<copy.h>

int main(int argc,char**argv){
     char*srcfile=argv[1];
	 char*destfile=argv[2];
      int offset=atoi(argv[3]);
	  int blocksize=atoi(argv[4]);
	  int srcfd,destfd;
	  srcfd=open(srcfile,O_RDONLY);
	  destfd=open(destfile,O_WRONLY|O_CREAT,0664);
	 lseek(srcfd,offset,SEEK_SET);
	 lseek(destfd,offset,SEEK_SET);
	 int len;
	 char buffer[blocksize];
     bzero(buffer,sizeof(buffer));
	 len=read(srcfd,buffer,sizeof(buffer));
	 write(destfd,buffer,len);
	 close(srcfd);
	 close(destfd);
	}
