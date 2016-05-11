#include "types.h"
#include "user.h"
#include "date.h"
//#include "lapic.c"
int get_secs();

int
main (int argc, char * argv[])
{

    //error check
    if(argc < 2) { 
	printf(2, "No arguments passed to time\n");
	exit();
    } 

    char ** args = argv+1;

    //get start time
    int start = get_secs(); 

    if(fork() == 0) { 
	exec(argv[1], args);
	printf(2, "exec failed running %s\n", argv[1] );
	exit(); 
     }
	wait();
	
    //get end time
    int finish = get_secs();  

    int total_time = finish - start; 

    printf(1, "%s ran in %d seconds \n", argv[1], total_time);

    

   exit();  
}

int get_secs() 
    {
    	struct rtcdate time;
    	date(&time);
	int hour = time.hour * 3600; 
	int min = time.minute * 60; 
	return time.second + min + hour;  
    }
