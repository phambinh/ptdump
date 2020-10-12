#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

typedef struct page_stats{
	int pid;
	long delay;
}pageStats;


void *spawnProcess(int pid, long delay){

	long num=1;
//	bool quit = false;

	while(1){

		char arg[64];
		char proc[32];
		sprintf(proc,"/proc/%d",pid);
		if( access(proc,F_OK)==-1)
			break;
		sprintf(arg,"pagetypes -p %d > pagestat%ld.%d",pid,num,pid);
		system(arg); 
		num++;
		sleep(delay);
	}

}

int main( int argc, char *argv[] ){
	
	//usage : dataCollect <pid> <Delay>
	if( argc != 3 ){
		printf( " Usage: dataCollect <pid> <delay> \n");
	}
	

 	int pid = atoi(argv[1]);
	long delay = atoi(argv[2]);
	printf("Spawn the process\n");
	spawnProcess(pid,delay);
}
