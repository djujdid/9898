#include<copy.h>

int process_wait(){
	
    pid_t zpid;
	while(zpid=wait(NULL)>0){
		printf("回收子进程pis=%d\n",zpid);
		}
	}
