#include "types.h"
#include "user.h"
#include "date.h"
//#include "lapic.c"

int
main (int argc, char * argv[])
{
    struct rtcdate r; 

    if (date(&r)) {
        printf (2, "date~failed\n");
	exit();  
    }


    printf(1, "\n %d Hours, %d Minutes,  %d Seconds,  %d Months,  %d Day, %d Year  \n ", 
		r.hour, r.minute, r.second, r.month, r.day, r.year);

   exit();  
}
