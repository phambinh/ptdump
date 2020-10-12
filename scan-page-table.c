#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

typedef struct page_stats{
	int pid;
	long delay;
}pageStats;


void spawnProcess(int pid, long delay){
    long num=1;

	while(1){
		char arg[64];
		char proc[32];
		sprintf(proc,"/proc/%d",pid);
		if( access(proc,F_OK)==-1) {
            printf("***** Application Finished. Stopped PT Scan Thread *****\n");
			break;
        }
		sprintf(arg,"./page-types -p %d > pagestat%ld.%d",pid,num,pid);
		system(arg); 
		num++;
		sleep(delay);
	}
}

int main( int argc, char *argv[] ){
	if( argc != 3 ){
		printf( " Usage: scan-page-table <pid> <delay> \n");
	}
	
 	int pid = atoi(argv[1]);
	long delay = atoi(argv[2]);
	printf("***** Started PT Scan Thread *****\n");
	spawnProcess(pid,delay);
    return 0;
}
