#include "types.h"
#include "user.h"
#include "date.h"

int
main (int argc, char * argv[])
{

  //if(argc != 2) {
    //printf(2, "Incorrect number of arguments: %d\n", argc); 
    //exit();
  //}
  int pid = atoi(argv[1]);
  int priority = atoi(argv[2]);

  printf(1, "User pid: %d, priority: %d\n", pid, priority);  
  int rc = setpriority(pid, priority);
  if(rc == -1) {
     printf(2, "set priority failed!\n");
     exit();
  }
  printf(1, "set priority worked!\n"); 
  exit();  
}
