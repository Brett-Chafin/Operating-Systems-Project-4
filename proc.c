#include "types.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "mmu.h"
#include "x86.h"
#include "proc.h"
#include "spinlock.h"
;

#define NEW_SCHEDS
struct {
  struct spinlock lock;
  struct proc proc[NPROC];
  struct proc * pReadyList[3];
  struct proc * pFreeList;
  uint   TimeToReset; 
} ptable;

static struct proc *initproc;

int nextpid = 1;
extern void forkret(void);
extern void trapret(void);


#ifdef NEW_SCHEDS
#define DEFAULT_PRIORITY 1
#define NUM_QUEUES 3
void addProcFreeList(struct proc * p); 
void addProcReadyList(struct proc * p); //adds a process to the back of the ready list
struct proc* getProcReadyList();
struct proc* getProcFreeList();
void printFreeList();
void printReadyList();
void initFreeList();
void initReadyList();
void getNumFreeList(); //print number of elements currently in the free list
void updateQueue(int pid, int priority);
void clockPriorityReset();
void resetPriorities(); 
void resetReadyQueues();
#endif
static void wakeup1(void *chan);
 

void
pinit(void)
{
  initlock(&ptable.lock, "ptable");
}

#ifdef NEW_SCHEDS
int modifypriority(int pid, int new_priority) 
{
  struct proc * p; 

  acquire(&ptable.lock); //needed for reading the ptable
  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
    if(p->pid == pid) {
      int oldPriority = p->priority;
      p->priority = new_priority;
      cprintf("Passing pid: %d, oldPriority: %d\n", pid, oldPriority);
      updateQueue(pid, oldPriority);
      release(&ptable.lock);
      return 0;
    }
  }
  release(&ptable.lock);
  return -1; //proc to modify was not found
}

/*helper function for updating what Q a proc is in when its prioirty if modified*/
void updateQueue(int pid, int priority) 
{
  struct proc * current = ptable.pReadyList[priority];
  struct proc * prev    = ptable.pReadyList[priority];

  //search the list looking for the desired process
  while(current->pid != pid && current != 0) {
    prev = current;
    current = current->next;
  }
  if(current == 0) {
    cprintf("UpdateQueue: proc not found\n");
    addProcReadyList(current);
    return;
  }
 
  //one node case
  if(prev->next == 0) {
    ptable.pReadyList[priority] = 0;
  }

  //if we find node in the middle of the list
  if(current->next != 0) {
    prev->next = current->next;  //append the list
  }

  //if we find our node at the beginning of a non empty list
  if(current == prev) {
    ptable.pReadyList[priority] = current->next;
  }
  
  //add desired node to the correct queue
  addProcReadyList(current);
  return;
}
#endif

//helper function for sys_getprocs
int get_current_procs(int max, struct uproc* table) 
{

   struct proc *p; 	//temp to get procs from table
   int i = 0; 		//index for max 
   
   //To get state string from the proc struct
  	static char *states[] = {
  	   [UNUSED]    "UNUSED",
 	   [EMBRYO]    "EMBRYO",
 	   [SLEEPING]  "SLEEP",
 	   [RUNNABLE]  "RUNABLE",
 	   [RUNNING]   "RUN   ",
  	   [ZOMBIE]    "ZOMBIE"
  	 };

  acquire(&ptable.lock);  //acquire lock when reading from the Ptable to prevent changes

  //run through the ptable and find all procs that are not marked unused. 
  //stop if end if reached, or the max requested is used
  for(p = ptable.proc; p < &ptable.proc[NPROC] && i < max; p++) {
    if(p->state != UNUSED) {
	
	table[i].uid = p->uid;
	table[i].gid = p->gid;
 	table[i].pid = p->pid;
	table[i].ppid = p->ppid;
	table[i].size = p->sz;
        table[i].priority = p->priority;
	++i;


	//strcpy for name
	unsigned k;
	for(k = 0; p->name[k] != '\0'; ++k) {
	  table[i].name[k] = p->name[k];
	}
	table[i].name[k] = '\0';

	//strcpy for state
      if(p->state >= 0 && p->state < NELEM(states) && states[p->state]) {
	char * state = states[p->state];
  	unsigned j;
  	for (j=0; state[j] != '\0'; ++j) {
    	  table[i].state[j] = state[j];
	}
  	table[i].state[j] = '\0';
      } 
    }
  }
  release(&ptable.lock);
  return i; 
}



//PAGEBREAK: 32
// Look in the process table for an UNUSED proc.
// If found, change state to EMBRYO and initialize
// state required to run in the kernel.
// Otherwise return 0.
static struct proc*
allocproc(void)
{
  struct proc *p;
  char *sp;

#ifdef NEW_SCHEDS
  acquire(&ptable.lock);

  //Get from Free List
  p = getProcFreeList();
  //if free list is empty
  if(p == 0) {	
    release(&ptable.lock); 
    return 0;
  }

#else
  acquire(&ptable.lock);
  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++)
    if(p->state == UNUSED)
      goto found;
  release(&ptable.lock);
  return 0;
#endif
  p->state = EMBRYO;
  if(nextpid == 1) {
    p->ppid = nextpid;
    p->gid  = FIRST_PROC_GID;
    p->uid  = FIRST_PROC_UID; 
  }
  else {
    p->ppid = proc->pid;
    p->gid  = proc->gid;
    p->uid  = proc->uid;
  } 

  p->pid = nextpid++;
  release(&ptable.lock);

  // Allocate kernel stack.
  if((p->kstack = kalloc()) == 0){
    p->state = UNUSED;

    #ifdef NEW_SCHEDS
    acquire(&ptable.lock); 
    addProcFreeList(p);
    release(&ptable.lock);
    #endif

    return 0;
  }
  sp = p->kstack + KSTACKSIZE;
  
  // Leave room for trap frame.
  sp -= sizeof *p->tf;
  p->tf = (struct trapframe*)sp;
  
  // Set up new context to start executing at forkret,
  // which returns to trapret.
  sp -= 4;
  *(uint*)sp = (uint)trapret;

  sp -= sizeof *p->context;
  p->context = (struct context*)sp;
  memset(p->context, 0, sizeof *p->context);
  p->context->eip = (uint)forkret;

  return p;
}

//PAGEBREAK: 32
// Set up first user process.
void
userinit(void)
{
  struct proc *p;
  extern char _binary_initcode_start[], _binary_initcode_size[];
 
  //initialize free list
  initFreeList();
  
  //init ready list
  initReadyList();
  
  //Cycles to reset procs to default priority 
  ptable.TimeToReset = 5000000; //5 million   
 
  p = allocproc();
  initproc = p;
  if((p->pgdir = setupkvm()) == 0)
    panic("userinit: out of memory?");
  inituvm(p->pgdir, _binary_initcode_start, (int)_binary_initcode_size);
  p->sz = PGSIZE;
  memset(p->tf, 0, sizeof(*p->tf));
  p->tf->cs = (SEG_UCODE << 3) | DPL_USER;
  p->tf->ds = (SEG_UDATA << 3) | DPL_USER;
  p->tf->es = p->tf->ds;
  p->tf->ss = p->tf->ds;
  p->tf->eflags = FL_IF;
  p->tf->esp = PGSIZE;
  p->tf->eip = 0;  // beginning of initcode.S

  safestrcpy(p->name, "initcode", sizeof(p->name));
  p->cwd = namei("/");

  p->state = RUNNABLE;
  #ifdef NEW_SCHEDS
  acquire(&ptable.lock);
  p->priority = DEFAULT_PRIORITY;
  addProcReadyList(p);
  release(&ptable.lock);
  #endif
}

//initialize free table **********************
#ifdef NEW_SCHEDS

/*returns the current number of free procs*/
void getNumFreeList() {
  int counter = 0;
  struct proc * current = ptable.pFreeList; 
  while(current->next != 0) { 
    counter++;
    current++;
  }
}


/*adds proc to the front of the free list*/
void addProcFreeList(struct proc * p) {
  //cprintf("Adding to the free list\n");
  
  if(!holding(&ptable.lock))
    panic("Not holding lock when adding to the free list\n");
  
  if(p->state != UNUSED)
    panic("adding process that is not unused");
  
  //insert p to the beginning of the free list
  p->next = ptable.pFreeList; 
  ptable.pFreeList = p;
  return;
}

/*Add procs to the front of the ready list*/
void addProcReadyList(struct proc * p) {
  if(p->state != RUNNABLE)
    panic("Trying to add a non-runnable state to the ready list");

  //cprintf("Adding proc %s to the ready list\n", p->name); 
  if(!holding(&ptable.lock))
    panic("Not holding lock when adding to ready list");

  //Handles all cases 
  p->next = ptable.pReadyList[p->priority];
  ptable.pReadyList[p->priority] = p;
  return; 
}

/*priority Q implementation*/ 
struct proc* getProcReadyList() {
  if(!holding(&ptable.lock))
    panic("Not holding lock when adding to ready list");
  
  int i; 
  for(i = 0; i < NUM_QUEUES; ++i) { 
    struct proc * current = ptable.pReadyList[i];
    struct proc * follow  = ptable.pReadyList[i];  

    if(current == 0) continue; //if the current queue is empty 
   
 
  //traverse to the end of the list with the follow pointer
    while(current->next != 0) {
      follow = current;
      current = current->next;
    }
    //delete current, the last proc in the list
    if(follow->next == 0)
      ptable.pReadyList[i] = 0;
    else 
      follow->next = 0;

    if(current->state != RUNNABLE) {
      panic("returning a proc that is not runnable!\n");
    }
    return current; //return the next proc to be ran 
  }
  return 0; //if we found nothing 
}


/*removes and returns the last proc in the ready list*/ 
/*
struct proc* getProcReadyList() {
  if(!holding(&ptable.lock))
    panic("Not holding lock when adding to ready list");
 
  struct proc * current = ptable.pReadyList;
  struct proc * follow  = ptable.pReadyList; 
  if(current == 0) return 0; //if the list is empty 
   
 
  //traverse to the end of the list with the follow pointer
  while(current->next != 0) {
	follow = current;
	current = current->next;
  }
  //delete current, the last proc in the list
  if(follow->next == 0)
    ptable.pReadyList = 0;
  else 
    follow->next = 0;

  if(current->state != RUNNABLE) {
    panic("returning a proc that is not runnable!\n");
   }
  return current; //return the next proc to be ran
}
*/

/*removes and returns the first proc in the free list*/ 
struct proc* getProcFreeList() {
  if(!holding(&ptable.lock))
    panic("Not holding lock when adding to ready list");

   //get the first proc in the list
   struct proc * current = ptable.pFreeList;
   
   //if list is empty
   if(current == 0) return 0;
 
   ptable.pFreeList = current->next;
   if(current->state != UNUSED) 
     panic("returning a free proc that is not unused!\n");
   //printFreeList();
   return current; 
}


/*initilizes the the free list to include all 64 procs*/ 
void initFreeList() {
  struct proc * p;
  ptable.pFreeList = ptable.proc;
  for(p = ptable.proc; p < &ptable.proc[NPROC - 1]; p++) {
    p->next = (p + 1);
  }
  p->next = 0;
}

void initReadyList() {
  int i;
  for(i = 0; i < NUM_QUEUES; ++i) {
    ptable.pReadyList[i] = 0;
  }
} 

void printReadyList() {
  int i; 
  for(i = 0; i < NUM_QUEUES; ++i) {
    struct proc * current = ptable.pReadyList[i];
    if(current == 0) {
      cprintf("Ready List[%d] is empty\n", i);
      continue;
    }
    cprintf("\nReady List[%d] includes: ", i);
    while(current != 0) {
      cprintf("Proc: %s, ", current->name);
      current = current->next;
    }
    cprintf(" end list\n");
  }
}

void printFreeList() {
  struct proc * current = ptable.pFreeList;
  if(current == 0) {
    cprintf("Free List is empty\n");
    return;
  }
  cprintf("Free List includes: "); 
  while(current != 0) {
    cprintf("Proc: %s, ", current->name);
    current = current->next;
  }
  cprintf(" end list\n");
} 

void clockPriorityReset() {

  if(!holding(&ptable.lock)) 
    panic("Not holding lock when resetting prioritys\n");
  if(ptable.TimeToReset > 0){ 
    ptable.TimeToReset -= 1;
    return; 
   }
  cprintf("Resetting priorities!\n");
  ptable.TimeToReset = 5000000; //5 million cycles ~= 5 seconds
  resetPriorities(); 
  resetReadyQueues();

  return; 
} 

void resetPriorities() {
  struct proc * p; 

  if(!holding(&ptable.lock))
    panic("Not holding lock when resetting the priorities!\n"); 
  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++) {
    if( p == 0) {
      cprintf("error!\n");
      return; //error
      
    }
    enum procstate s = p->state;
    if(s != RUNNABLE || s != SLEEPING || s != RUNNING)
      continue;
    proc->priority = DEFAULT_PRIORITY; 
  }
}

void resetReadyQueues() {
 if(!holding(&ptable.lock))
    panic("Not holding lock when resetting the ready queues!\n");  

  struct proc * p;
  struct proc * temp;
  int i; 

  //loop through all queues
  for(i = 0; i < NUM_QUEUES; ++i) {
    
    //skip default queue
    if(i == DEFAULT_PRIORITY)
      continue;
    
    //add all nodes on current list to default priority list
    p = ptable.pReadyList[i]; 
    while(p != 0) {
      temp = p->next; 
      addProcReadyList(p);
      p = temp;
    }
    //delete the list
    ptable.pReadyList[i] = 0;
  }
}

#endif

// Grow current process's memory by n bytes.
// Return 0 on success, -1 on failure.
int
growproc(int n)
{
  uint sz;
  
  sz = proc->sz;
  if(n > 0){
    if((sz = allocuvm(proc->pgdir, sz, sz + n)) == 0)
      return -1;
  } else if(n < 0){
    if((sz = deallocuvm(proc->pgdir, sz, sz + n)) == 0)
      return -1;
  }
  proc->sz = sz;
  switchuvm(proc);
  return 0;
}

// Create a new process copying p as the parent.
// Sets up stack to return as if from system call.
// Caller must set state of returned proc to RUNNABLE.
int
fork(void)
{
  int i, pid;
  struct proc *np;

  // Allocate process.
  if((np = allocproc()) == 0)
    return -1;

  // Copy process state from p.
  if((np->pgdir = copyuvm(proc->pgdir, proc->sz)) == 0){
    kfree(np->kstack);
    np->kstack = 0;
    np->state = UNUSED;
    #ifdef NEW_SCHEDS
    acquire(&ptable.lock);
    np->priority = DEFAULT_PRIORITY;
    addProcFreeList(np);
    release(&ptable.lock);
    #endif
    return -1;
  }
  np->sz = proc->sz;
  np->parent = proc;
  *np->tf = *proc->tf;

  // Clear %eax so that fork returns 0 in the child.
  np->tf->eax = 0;

  for(i = 0; i < NOFILE; i++)
    if(proc->ofile[i])
      np->ofile[i] = filedup(proc->ofile[i]);
  np->cwd = idup(proc->cwd);

  safestrcpy(np->name, proc->name, sizeof(proc->name));
 
  pid = np->pid;

  // lock to force the compiler to emit the np->state write last.
  acquire(&ptable.lock);
  np->state = RUNNABLE;
  #ifdef NEW_SCHEDS
  np->priority = DEFAULT_PRIORITY;
  addProcReadyList(np);
  #endif
  release(&ptable.lock);
  
  return pid;
}

// Exit the current process.  Does not return.
// An exited process remains in the zombie state
// until its parent calls wait() to find out it exited.
void
exit(void)
{
  struct proc *p;
  int fd;

  if(proc == initproc)
    panic("init exiting");

  // Close all open files.
  for(fd = 0; fd < NOFILE; fd++){
    if(proc->ofile[fd]){
      fileclose(proc->ofile[fd]);
      proc->ofile[fd] = 0;
    }
  }

  begin_op();
  iput(proc->cwd);
  end_op();
  proc->cwd = 0;

  acquire(&ptable.lock);

  // Parent might be sleeping in wait().
  wakeup1(proc->parent);

  // Pass abandoned children to init.
  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
    if(p->parent == proc){
      p->parent = initproc;
      if(p->state == ZOMBIE)
        wakeup1(initproc);
    }
  }

  // Jump into the scheduler, never to return.
  proc->state = ZOMBIE;
  sched();
  panic("zombie exit");
}

// Wait for a child process to exit and return its pid.
// Return -1 if this process has no children.
int
wait(void)
{
  struct proc *p;
  int havekids, pid;

  acquire(&ptable.lock);
  for(;;){
    // Scan through table looking for zombie children.
    havekids = 0;
    for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
      if(p->parent != proc)
        continue;
      havekids = 1;
      if(p->state == ZOMBIE){
        // Found one.
        pid = p->pid;
        kfree(p->kstack);
        p->kstack = 0;
        freevm(p->pgdir);
        p->state = UNUSED;
	#ifdef NEW_SCHEDS
 	addProcFreeList(p);	//add the proc back to the free list to be reused
        #endif
        p->pid = 0;
        p->parent = 0;
        p->name[0] = 0;
        p->killed = 0; 
        release(&ptable.lock);
        return pid;
      }
    }

    // No point waiting if we don't have any children.
    if(!havekids || proc->killed){
      release(&ptable.lock);
      return -1;
    }

    // Wait for children to exit.  (See wakeup1 call in proc_exit.)
    sleep(proc, &ptable.lock);  //DOC: wait-sleep
  }
} 

//PAGEBREAK: 42
// Per-CPU process scheduler.
// Each CPU calls scheduler() after setting itself up.
// Scheduler never returns.  It loops, doing:
//  - choose a process to run
//  - swtch to start running that process
//  - eventually that process transfers control
//      via swtch back to the scheduler.
#ifdef NEW_SCHEDS
//new scheduler
void
scheduler(void)
{
  struct proc *p;
  for(;;){
      // Enable interrupts on this processor.
      sti();

      acquire(&ptable.lock);
      //cprintf("About to enter the getreadylist() function\n");
      if((p = getProcReadyList()) != 0) {  
      //cprintf("Coming out of the getreadylist() function\n");
    
      //if there is no processes on the ready list
      // Switch to chosen process.  It is the process's job
      // to release ptable.lock and then reacquire it
      // before jumping back to us.
     
      proc = p;
      switchuvm(p);
      p->state = RUNNING;
      swtch(&cpu->scheduler, proc->context);
      switchkvm();

      // Process is done running for now.
      // It should have changed its p->state before coming back.
      proc = 0;
    }
    clockPriorityReset();
    release(&ptable.lock);
  }
}

#else
//old scheduler
void
scheduler(void)
{
  struct proc *p;

  for(;;){
    // Enable interrupts on this processor.
    sti();

    // Loop over process table looking for process to run.
    acquire(&ptable.lock);
    for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
      if(p->state != RUNNABLE)
        continue;

      // Switch to chosen process.  It is the process's job
      // to release ptable.lock and then reacquire it
      // before jumping back to us.
      proc = p;
      switchuvm(p);
      p->state = RUNNING;
      swtch(&cpu->scheduler, proc->context);
      switchkvm();

      // Process is done running for now.
      // It should have changed its p->state before coming back.
      proc = 0;
    }
    release(&ptable.lock);

  }
}
#endif

// Enter scheduler.  Must hold only ptable.lock
// and have changed proc->state.
void
sched(void)
{
  int intena;

  if(!holding(&ptable.lock))
    panic("sched ptable.lock");
  if(cpu->ncli != 1)
    panic("sched locks");
  if(proc->state == RUNNING)
    panic("sched running");
  if(readeflags()&FL_IF)
    panic("sched interruptible");
  intena = cpu->intena;
  swtch(&proc->context, cpu->scheduler);
  cpu->intena = intena;
}

// Give up the CPU for one scheduling round.
void
yield(void)
{
  acquire(&ptable.lock);  //DOC: yieldlock
  proc->state = RUNNABLE;
  addProcReadyList(proc);
  sched();
  release(&ptable.lock);
}

// A fork child's very first scheduling by scheduler()
// will swtch here.  "Return" to user space.
void
forkret(void)
{
  static int first = 1;
  // Still holding ptable.lock from scheduler.
  release(&ptable.lock);

  if (first) {
    // Some initialization functions must be run in the context
    // of a regular process (e.g., they call sleep), and thus cannot 
    // be run from main().
    first = 0;
    iinit(ROOTDEV);
    initlog(ROOTDEV);
  }
  
  // Return to "caller", actually trapret (see allocproc).
}

// Atomically release lock and sleep on chan.
// Reacquires lock when awakened.
void
sleep(void *chan, struct spinlock *lk)
{
  if(proc == 0)
    panic("sleep");

  if(lk == 0)
    panic("sleep without lk");

  // Must acquire ptable.lock in order to
  // change p->state and then call sched.
  // Once we hold ptable.lock, we can be
  // guaranteed that we won't miss any wakeup
  // (wakeup runs with ptable.lock locked),
  // so it's okay to release lk.
  if(lk != &ptable.lock){  //DOC: sleeplock0
    acquire(&ptable.lock);  //DOC: sleeplock1
    release(lk);
  }

  // Go to sleep.
  proc->chan = chan;
  proc->state = SLEEPING;
  sched();

  // Tidy up.
  proc->chan = 0;

  // Reacquire original lock.
  if(lk != &ptable.lock){  //DOC: sleeplock2
    release(&ptable.lock);
    acquire(lk);
  }
}

//PAGEBREAK!
// Wake up all processes sleeping on chan.
// The ptable lock must be held.
static void
wakeup1(void *chan)
{
  struct proc *p;

  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++)
    if(p->state == SLEEPING && p->chan == chan) {
      p->state = RUNNABLE;
      #ifdef NEW_SCHEDS
      addProcReadyList(p);
      #endif
   }
}

// Wake up all processes sleeping on chan.
void
wakeup(void *chan)
{
  acquire(&ptable.lock);
  wakeup1(chan);
  release(&ptable.lock);
}

// Kill the process with the given pid.
// Process won't exit until it returns
// to user space (see trap in trap.c).
int
kill(int pid)
{
  struct proc *p;

  acquire(&ptable.lock);
  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
    if(p->pid == pid){
      p->killed = 1;
      // Wake process from sleep if necessary.
      if(p->state == SLEEPING) {
        p->state = RUNNABLE;
	#ifdef NEW_SCHEDS
        addProcReadyList(p); //add process to ready list
        #endif
      }
      release(&ptable.lock);
      return 0;
    }
  }
  release(&ptable.lock);
  return -1;
}

//PAGEBREAK: 36
// Print a process listing to console.  For debugging.
// Runs when user types ^P on console.
// No lock to avoid wedging a stuck machine further.
void
procdump(void)
{
  static char *states[] = {
  [UNUSED]    "unused",
  [EMBRYO]    "embryo",
  [SLEEPING]  "sleep ",
  [RUNNABLE]  "runble",
  [RUNNING]   "run   ",
  [ZOMBIE]    "zombie"
  };
  int i;
  struct proc *p;
  char *state;
  uint pc[10];
 
 
  printReadyList(); 
  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
    if(p->state == UNUSED)
      continue;
    if(p->state >= 0 && p->state < NELEM(states) && states[p->state])
      state = states[p->state];
    else
      state = "???";
    cprintf("Process name: %s, state:  %s, pid: %d, gid: %d, uid: %d, priority: %d \n ", p->name, state, p->pid, p->gid, p->uid, p->priority);
    if(p->state == SLEEPING){
      getcallerpcs((uint*)p->context->ebp+2, pc);
      for(i=0; i<10 && pc[i] != 0; i++)
        cprintf(" %p", pc[i]);
    }
    cprintf("\n");
  }
}

