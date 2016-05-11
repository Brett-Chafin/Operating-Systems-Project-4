#include "types.h"
#include "user.h"
#include "BC_ps.h"

#define MAX_PROCESS_TABLE 64   //64 because this is the max number of active procs
;
int
main (int argc, char * argv[])
{
   int max = MAX_PROCESS_TABLE;  
   struct uproc table[max]; 

    int procs_recieved = getprocs(max, table);  

    if(procs_recieved == -1) printf(1, "The system call getprocs failed.\n");

    if(procs_recieved == 0) printf(1, "System call getprocs() returned no processes.\n");
 
    int i;
    for(i = 1; i < procs_recieved; i++) {
	printf(1, "Process %d: Name: %s, ", i, table[i].name);
        printf(1, "pid: %d,  ", table[i].pid);
	printf(1, "uid: %d,  ", table[i].uid);
	printf(1, "gid: %d,  ", table[i].gid);
	printf(1, "ppid: %d,  ", table[i].ppid);
	printf(1, "size: %d,  ", table[i].size);
	printf(1, "state: %s, \n", table[i].state);
    }

   exit();  
}
