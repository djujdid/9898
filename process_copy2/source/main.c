#include<copy.h>
int main(int argc,char** argv){
	    int pronum;
		int blocksize;
	    if(argc < 4) {
			pronum = 1;
			}else {
		  pronum=atoi(argv[3]); }
          blocksize = 100; 													    process_create(argv[1],argv[2],pronum,blocksize);

																				    return 0;
}
