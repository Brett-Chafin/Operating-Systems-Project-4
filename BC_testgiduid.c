#include "types.h"
#include "user.h"

int
main (int argc, char * argv[]) 
{
   	int uid, gid, ppid; 
	
	uid = getuid();
	printf(2, "Current_UID_is : _%d\n", uid); 
	printf(2, "Setting_UID_to_100\n" ); 
	setuid(100);
	uid = getuid();
	printf(2, "Current_UID_is: %d\n", uid);

	gid = getgid();
	printf(2, "Current_GID_is : _%d\n", gid); 
	printf(2, "Setting_GID_to_100\n" ); 
	setgid(100);
	gid = getgid();
	printf(2, "Current_GID_is: %d\n", gid);

	ppid = getppid();
    	printf(2, "My_parent_process_is: %d\n", ppid);
	printf(2, "Done!\n");
	
	exit(); 
}
