#include "types.h"
#include "x86.h"
#include "defs.h"
#include "date.h"
#include "param.h"
#include "memlayout.h"
#include "mmu.h"
#include "proc.h"
;
int
sys_fork(void)
{
  return fork();
}

int
sys_exit(void)
{
  exit();
  return 0;  // not reached
}

int
sys_wait(void)
{
  return wait();
}

int
sys_kill(void)
{
  int pid;

  if(argint(0, &pid) < 0)
    return -1;
  return kill(pid);
}

int
sys_getpid(void)
{
  return proc->pid;
}

int
sys_sbrk(void)
{
  int addr;
  int n;

  if(argint(0, &n) < 0)
    return -1;
  addr = proc->sz;
  if(growproc(n) < 0)
    return -1;
  return addr;
}

int
sys_sleep(void)
{
  int n;
  uint ticks0;
  
  if(argint(0, &n) < 0)
    return -1;
  acquire(&tickslock);
  ticks0 = ticks;
  while(ticks - ticks0 < n){
    if(proc->killed){
      release(&tickslock);
      return -1;
    }
    sleep(&ticks, &tickslock);
  }
  release(&tickslock);
  return 0;
}

// return how many clock tick interrupts have occurred
// since start.
int
sys_uptime(void)
{
  uint xticks;
  
  acquire(&tickslock);
  xticks = ticks;
  release(&tickslock);
  return xticks;
}

//Turn of the computer
int sys_halt(void){
  cprintf("Shutting down ...\n");
  outw (0xB004, 0x0 | 0x2000);
  return 0;
}

int 
sys_date(void) 
{
    struct rtcdate * d;
   
    if(argptr(0, (void*)&d, sizeof(*d)) < 0)
	return -1; 

   cmostime(d); 
	
   return 0;
}

int
sys_getuid(void) 
{ 
  return proc->uid; 
} 

int 
sys_getgid(void) 
{
  return proc->gid;
} 

int
sys_getppid(void) 
{ 
  return proc->ppid; 
}

int 
sys_setuid(void) 
{ 
  int new_uid;

  if(argint(0, &new_uid) < 0)  //test if argint failed
    return -1;

  proc->uid = new_uid;
    return 0;   
}

int
sys_setgid(void) 
{ 
  int new_gid;

  if(argint(0, &new_gid) < 0)  //test if argint failed
      return -1;

  proc->gid = new_gid;
    return 0; 
}

int
sys_getprocs(void) 
{

  struct uproc * table;
  int max; 

  //get int max arg off the stack
  if(argint(0, &max) < 0) 
    return -1;  
   
  //get uproc structs off the stack
  if(argptr(1, (void*)&table, sizeof(*table)) < 0)
    return -1; 

  int procs_retrieved =get_current_procs(max, table);  
	
  return procs_retrieved;
}

int 
sys_setpriority(void) 
{
  int pid;
  int new_priority;

  if(argint(0, &pid) < 0) 
    return -1;  
   
  if(argint(1, &new_priority) < 0) 
    return -1;

  cprintf("pid: %d, new_priority: %d \n", pid, new_priority);
  
  if(new_priority > 2 || new_priority < 0) {
    cprintf("Invalid Priority!\n");
    return -1;
  }
 
  //set priority helper
  int rc = modifypriority(pid, new_priority);
  return rc;
}



