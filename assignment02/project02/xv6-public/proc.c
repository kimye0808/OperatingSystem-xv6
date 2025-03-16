#include "types.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "mmu.h"
#include "x86.h"
#include "proc.h"
#include "spinlock.h"


struct {
  struct spinlock lock;
  struct proc proc[NPROC];
} ptable;

static struct proc *initproc;

int nextpid = 1;
int nextTid = 1;/*for project02*/

extern void forkret(void);
extern void trapret(void);

static void wakeup1(void *chan);

void
pinit(void)
{
  initlock(&ptable.lock, "ptable");
}

// Must be called with interrupts disabled
int
cpuid() {
  return mycpu()-cpus;
}

// Must be called with interrupts disabled to avoid the caller being
// rescheduled between reading lapicid and running through the loop.
struct cpu*
mycpu(void)
{
  int apicid, i;
  
  if(readeflags()&FL_IF)
    panic("mycpu called with interrupts enabled\n");
  
  apicid = lapicid();
  // APIC IDs are not guaranteed to be contiguous. Maybe we should have
  // a reverse map, or reserve a register to store &cpus[i].
  for (i = 0; i < ncpu; ++i) {
    if (cpus[i].apicid == apicid)
      return &cpus[i];
  }
  panic("unknown apicid\n");
}

// Disable interrupts so that we are not rescheduled
// while reading proc from the cpu structure
struct proc*
myproc(void) {
  struct cpu *c;
  struct proc *p;
  pushcli();
  c = mycpu();
  p = c->proc;
  popcli();
  return p;
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

  acquire(&ptable.lock);

  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++)
    if(p->state == UNUSED)
      goto found;

  release(&ptable.lock);
  return 0;

found:
  p->state = EMBRYO;
  p->pid = nextpid++;
  /*for project02*/
  p->mem_limit = 0;
  p->sPage_cnt = 0;
  p->tid = 0;
  p->retval = 0;
  p->front = 0;
  p->rear = 0;
  p->tmaker = p;

  release(&ptable.lock);

  // Allocate kernel stack.
  if((p->kstack = kalloc()) == 0){
    p->state = UNUSED;
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

  // this assignment to p->state lets other cores
  // run this process. the acquire forces the above
  // writes to be visible, and the lock is also needed
  // because the assignment might not be atomic.
  acquire(&ptable.lock);

  p->state = RUNNABLE;

  release(&ptable.lock);
}

// Grow current process's memory by n bytes.
// Return 0 on success, -1 on failure.
int
growproc(int n)
{
  uint sz;
  struct proc *curproc = myproc();

  acquire(&ptable.lock);
  sz = curproc->sz;
  /*for project02*/
  if(!(curproc->mem_limit > 0 && sz >= curproc->mem_limit)) 
  {
    if(n > 0){
      if((sz = allocuvm(curproc->pgdir, sz, sz + n)) == 0){
  //cprintf("allocuvm !pid: %d, tid: %d, sz: %d\n", curproc->pid, curproc->tid, curproc->sz);
        release(&ptable.lock);
        return -1;
      }
    } else if(n < 0){
      if((sz = deallocuvm(curproc->pgdir, sz, sz + n)) == 0){
        release(&ptable.lock);
        return -1;
      }
    }
 //cprintf("pid: %d, tid: %d, sz: %d\n", curproc->pid, curproc->tid, curproc->sz);

    /*for project02 : set memory limit*/
    if(curproc->mem_limit > 0 && sz > curproc->mem_limit) {
      sz = deallocuvm(curproc->pgdir, sz+n, sz);
    }
  }
  release(&ptable.lock);

  if(curproc->tid){/*for sysproc.c : sbrk systemcall*/
    thread_family_sz_same(curproc->pid, sz);
  }
 //cprintf("pid: %d, tid: %d, sz: %d\n", curproc->pid, curproc->tid, curproc->sz);

  curproc->sz = sz;
  switchuvm(curproc);
  return 0;
}

/**************************
          project02
process memory limitation
****************************/
int 
setmemorylimit(int pid, int limit)
{
  if(limit<0) return -1;/*if limit minus return -1*/
  //else if(limit==0) return 0;/*if limit = 0, there is no limitation,,,,,limit 0은 초기값인듯?*/
  struct proc *p;
  acquire(&ptable.lock);
  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++) 
  {
    if(p->pid == pid && p->tmaker == p) 
    {
      if(p->sz > limit)/*if limit is smaller than present mem alloc of proc, return -1*/
      {
        release(&ptable.lock);
        return -1;
      }
      p->mem_limit = limit;/*it's normal*/
      release(&ptable.lock);
      return 0;
    }
  }
  release(&ptable.lock);
  return -1; /*cannot find proc*/
}

/********************************
            project02
list of RUNNING & RUNNABLE process
*********************************/
void
listProc(void){
  struct proc *p;

  acquire(&ptable.lock);
  cprintf("<list of RUNNING or RUNNABLE process>\n");
  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
    if(p->tmaker != p) continue;
    if(p->state != RUNNABLE && p->state != RUNNING)
      continue;
    cprintf("<pname: %s | pid: %d | stack pages: %d | assigned_mem: %d | mem_limit: %d>\n", 
              p->name, p->pid, p->sPage_cnt, p->sz, p->mem_limit);/*mem size of proc include thread;; maybe*/
  }
  release(&ptable.lock);
}
// Create a new process copying p as the parent.
// Sets up stack to return as if from system call.
// Caller must set state of returned proc to RUNNABLE.
int
fork(void)
{
  int i, pid;
  struct proc *np;
  struct proc *curproc = myproc();

  // Allocate process.
  if((np = allocproc()) == 0){
    return -1;
  }
  //cprintf("in fork:ppid: %d, tid: %d sz: %d, stack:%d\n", curproc->pid, curproc->tid, curproc->sz, curproc->thdva);
  
  // Copy process state from proc.
  acquire(&ptable.lock);//

  if(!curproc->tid && curproc->front == curproc->rear){//if not has spare list, there is no mem leak
    if((np->pgdir = copyuvm(curproc->pgdir, curproc->sz)) == 0){
      kfree(np->kstack);
      np->kstack = 0;
      np->state = UNUSED;
      return -1;
    }
    np->front = 0;
    np->rear = 0;
  }else{//if there is spare list, copy without that area
    struct proc *procmaker = curproc->tmaker;
    if((np->pgdir = copyuvmTHD(procmaker->pgdir,procmaker->sz, procmaker->spare, procmaker->front, procmaker->rear))==0){
      kfree(np->kstack);
      np->kstack = 0;
      np->state = UNUSED;
      return -1;
    }
    //copy spare array 
    np->front = 0;
    np->rear = 0;
    int pfront = procmaker->front;
    int prear = procmaker->rear;
    int casefr = prear - pfront;
    if(casefr > 0){//rear > front
      for(int idx = pfront; idx < prear; idx++){
        np->spare[np->rear] = procmaker->spare[idx];
        np->rear = (np->rear+1) % NTHREAD;
      }
    }else if(casefr < 0){ //front > rear
      for(int idx = pfront; idx < NTHREAD; idx++){
        np->spare[np->rear] = procmaker->spare[idx];
        np->rear = (np->rear+1) % NTHREAD;
      }
      for(int idx = 0; idx <= prear; idx++){
        np->spare[np->rear] = procmaker->spare[idx];
        np->rear = (np->rear+1) % NTHREAD;
      }
    }
  }
  /*for project02*/
 // np->tid = 0;
  np->sz = curproc->sz;
  np->parent = curproc;
  np->tmaker = np;
  *np->tf = *curproc->tf;
  np->mem_limit = curproc->mem_limit;
  np->sPage_cnt = curproc->sPage_cnt;

  // Clear %eax so that fork returns 0 in the child.
  np->tf->eax = 0;
  release(&ptable.lock);//

  for(i = 0; i < NOFILE; i++)
    if(curproc->ofile[i])
      np->ofile[i] = filedup(curproc->ofile[i]);
  np->cwd = idup(curproc->cwd);

  safestrcpy(np->name, curproc->name, sizeof(curproc->name));

  pid = np->pid;

  acquire(&ptable.lock);

  np->state = RUNNABLE;

  release(&ptable.lock);

  return pid;
}

// Exit the current process.  Does not return.
// An exited process remains in the zombie state
// until its parent calls wait() to find out it exited.
void
exit(void)
{
  struct proc *curproc = myproc();
  struct proc *p;
  int fd;

  if(curproc == initproc)
    panic("init exiting");
      // cprintf("exit: tid:%d, pid:%d, sz: %d , thdva: %d\n", myproc()->tid, myproc()->pid, myproc()->sz, myproc()->thdva);
	thread_cleaner(curproc);
  // Close all open files.
  for(fd = 0; fd < NOFILE; fd++){
    if(curproc->ofile[fd]){
      fileclose(curproc->ofile[fd]);
      curproc->ofile[fd] = 0;
    }
  }

  begin_op();
  iput(curproc->cwd);
  end_op();
  curproc->cwd = 0;

  acquire(&ptable.lock);

  // Parent might be sleeping in wait().
  wakeup1(curproc->parent);

  // Pass abandoned children to init.
  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
    if(p->parent == curproc){
      p->parent = initproc;
      if(p->state == ZOMBIE)
        wakeup1(initproc);
    }
  }

  // Jump into the scheduler, never to return.
  curproc->state = ZOMBIE;
  // cprintf("exit done %d  %d %d\n",curproc->pid, curproc->parent->pid, curproc->tmaker->pid);
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
  //cprintf("before call myproc\n");
  struct proc *curproc = myproc();
    //    cprintf("after myproc pid: %d, tid: %d\n", curproc->pid, curproc->tid);
  //cprintf("wait!!!!!!!!!!!1\n");
  acquire(&ptable.lock);
  for(;;){
    // Scan through table looking for exited children.
    havekids = 0;
    for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
      if(p->parent != curproc)
        continue;
      havekids = 1;
      if(p->state == ZOMBIE){
        // Found one.
        pid = p->pid;
        //cprintf("before kfree pid: %d, tid: %d\n", p->pid, p->tid);
        kfree(p->kstack);
        //cprintf("after kfree pid: %d, tid: %d\n", p->pid, p->tid);

        p->kstack = 0;
        freevmTHD(p->pgdir, p->tmaker->spare, p->tmaker->front, p->tmaker->rear);
        p->pid = 0;
        p->parent = 0;
        p->name[0] = 0;
        p->killed = 0;
        p->front = 0;
        p->rear = 0;
        p->tmaker = 0;
        p->state = UNUSED;
        p->thdva = 0;
        p->sz = 0;
        release(&ptable.lock);
        return pid;
      }
    }
    
    // No point waiting if we don't have any children.
    if(!havekids || curproc->killed){
      release(&ptable.lock);
      return -1;
    }

    // Wait for children to exit.  (See wakeup1 call in proc_exit.)
    sleep(curproc, &ptable.lock);  //DOC: wait-sleep
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
void
scheduler(void)
{
  struct proc *p;
  struct cpu *c = mycpu();
  c->proc = 0;

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
      c->proc = p;
      switchuvm(p);
      p->state = RUNNING;
     //cprintf("scheduler pid : %d, tid: %d , sz: %d\n", p->pid, p->tid, p->sz);

      swtch(&(c->scheduler), p->context);
      switchkvm();
      // Process is done running for now.
      // It should have changed its p->state before coming back.
      c->proc= 0;
    }
    release(&ptable.lock);

  }
}


// Enter scheduler.  Must hold only ptable.lock
// and have changed proc->state. Saves and restores
// intena because intena is a property of this
// kernel thread, not this CPU. It should
// be proc->intena and proc->ncli, but that would
// break in the few places where a lock is held but
// there's no process.
void
sched(void)
{
  int intena;
  struct proc *p = myproc();

  if(!holding(&ptable.lock))
    panic("sched ptable.lock");
  if(mycpu()->ncli != 1)
    panic("sched locks");
  if(p->state == RUNNING)
    panic("sched running");
  if(readeflags()&FL_IF)
    panic("sched interruptible");
  intena = mycpu()->intena;
  swtch(&p->context, mycpu()->scheduler);
 // cprintf("exit sched! pid: %d tid: %d \n", p->pid, p->tid);

  mycpu()->intena = intena;
}

// Give up the CPU for one scheduling round.
void
yield(void)
{
  acquire(&ptable.lock);  //DOC: yieldlock
  myproc()->state = RUNNABLE;
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
  struct proc *p = myproc();
  
  if(p == 0)
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
  p->chan = chan;
  p->state = SLEEPING;

  sched();

  // Tidy up.
  p->chan = 0;

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
    if(p->state == SLEEPING && p->chan == chan)
      p->state = RUNNABLE;
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
  int killFlag = 0;//for general case 

  acquire(&ptable.lock);
  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
    if(p->pid == pid){
      p->killed = 1;
      if(p->state == SLEEPING)
        p->state = RUNNABLE;
      //release(&ptable.lock);
      //return 0; /*for killing thread*/
      killFlag = 1;//
    }
  }
  if(killFlag){
    release(&ptable.lock);
    return 0;
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

  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
    if(p->state == UNUSED)
      continue;
    if(p->state >= 0 && p->state < NELEM(states) && states[p->state])
      state = states[p->state];
    else
      state = "???";
    cprintf("%d %s %s", p->pid, state, p->name);
    if(p->state == SLEEPING){
      getcallerpcs((uint*)p->context->ebp+2, pc);
      for(i=0; i<10 && pc[i] != 0; i++)
        cprintf(" %p", pc[i]);
    }
    cprintf("\n");
  }
}




/********************************
            project02
          create thread
*********************************/
int 
thread_create(thread_t *thread, void *(*start_routine)(void *), void *arg)/*thread : copy then alloc stack page*/
{
  int i;
  int front, rear; int hasSpare = 0;
  uint sp, sz;
  uint ustack[2];/*user stack for arg, fake PC*/
  Thread *nThread;
  struct proc *curproc = myproc();

  /********************************
  thread_create- copy proc to thread
  *********************************/
  if((nThread = allocproc()) == 0){
    return -1;
  }

  acquire(&ptable.lock);
  /*copy to thread : page direction to page table*/
  nThread->pgdir = curproc->pgdir;
  if(nThread->pgdir == 0){
    goto bad;
  }

  /*copy to thread : size of proc memory and trap frame*/
  *nThread->tf = *curproc->tf;
  nThread->parent = curproc->parent;
  nThread->pid = curproc->pid;/*pid of thread is (parent) process*/
  
    // Clear %eax so that fork returns 0 in the child.
  nThread->tf->eax = 0;
  release(&ptable.lock);

    /*copy to thread : current directory and file descriptors*/
  for(i = 0; i < NOFILE; i++)
    if(curproc->ofile[i])
      nThread->ofile[i] = filedup(curproc->ofile[i]);
  nThread->cwd = idup(curproc->cwd);

  acquire(&ptable.lock);
    /*copy to thread : name*/
  safestrcpy(nThread->name, curproc->name, sizeof(curproc->name));
  release(&ptable.lock);

  /********************************
  thread_create-allocate stack pages
  *********************************/
  acquire(&ptable.lock);
  front = curproc->front;
  rear = curproc->rear;
  hasSpare = (front != rear);
  if(hasSpare){
    sz = curproc->spare[front];
  }else{
    sz = curproc->sz;
  }

  sz = PGROUNDUP(sz);
  if((sz = allocuvm(nThread->pgdir, sz, sz + 2*PGSIZE)) == 0)
    goto bad;
  clearpteu(nThread->pgdir, (char*)(sz - 2*PGSIZE));
  
  sp = sz;

  ustack[0] = 0xffffffff;  // fake return PC
  ustack[1] = (uint)arg;

  sp -= (2) * 4;
  if(copyout(nThread->pgdir, sp, ustack, 2*4) < 0)/*ustack to pgdir mem*/
    goto bad;

  /*setting start_routine and thread info*/
  if(hasSpare){
    curproc->sz = curproc->sz;//not change
    curproc->front = (front+1) % NTHREAD;
  }else{
    curproc->sz = sz;//added version update
  }
  nThread->sz = curproc->sz;
  nThread->thdva = sz - 2*PGSIZE;

  nThread->tf->eip = (uint)start_routine;
  nThread->tf->esp = sp;
  nThread->tid = nextTid++;
  nThread->tmaker = curproc;

  *thread = nThread->tid;/*set thread argument*/
  nThread->state = RUNNABLE;
  release(&ptable.lock);

  //cprintf("pid: %d , tid: %d, sz: %d\n", nThread->pid, nThread->tid, nThread->sz);
  thread_family_sz_same(curproc->pid, curproc->sz);

  acquire(&ptable.lock);

  nThread->state = RUNNABLE;
  release(&ptable.lock);
  return 0;

  bad:
    release(&ptable.lock);
    kfree(nThread->kstack);
    nThread->kstack = 0;
    nThread->state = UNUSED;
    return -1;
}

/********************************
            project02
           thread exit
*********************************/
void 
thread_exit(void *retval){/*thread : exit*/
  Thread *curThd = myproc();
  struct proc *p;
  int fd;

  acquire(&ptable.lock);
  // Close all open files.
  for(fd = 0; fd < NOFILE; fd++){
    if(curThd->ofile[fd]){
      release(&ptable.lock);
      fileclose(curThd->ofile[fd]);
      acquire(&ptable.lock);
      curThd->ofile[fd] = 0;
    }
  }
  release(&ptable.lock);

  begin_op();
  iput(curThd->cwd);
  end_op();
  acquire(&ptable.lock);
  curThd->cwd = 0;

     //  cprintf("thread_exit: tid:%d, pid:%d, sz: %d , thdva: %d\n", myproc()->tid, myproc()->pid, myproc()->sz, myproc()->thdva);

  wakeup1((void*)curThd->tid);
  //cprintf("wakeup ticks:%d  pid: %d, tid: %d , sz: %d, thdva: %d\n", ticks, curThd->pid, curThd->tid, curThd->sz, curThd->thdva);
    // Pass abandoned children to init.
  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
    if(p->parent == curThd){
      p->parent = initproc;
      if(p->state == ZOMBIE)
        wakeup1(initproc);
    }
  }
  curThd->retval = retval;
  curThd->state = ZOMBIE;
  sched();  
  //cprintf("zombie exit!!! ticks:%d  pid: %d, tid: %d , sz: %d, thdva: %d\n", ticks, curThd->pid, curThd->tid, curThd->sz, curThd->thdva);
  panic("zombie exit");
}

/********************************
            project02
          thread  join 
*********************************/
int 
thread_join(thread_t thread, void **retval){/*thread : wait*/
  Thread *thd;
  int isThereThd;/*thread arg flag*/
  struct proc *curproc = myproc();
  
  acquire(&ptable.lock);
  struct proc *curprocPar = curproc->tmaker;
  //cprintf("join before enter thread: %d ,tid: %d \n", thread, curproc->tid);
  for(;;){
    isThereThd = 0;
    for(thd = ptable.proc; thd < &ptable.proc[NPROC]; thd++){
      if(thd->tid != thread)
        continue;
      if(thd->state == UNUSED){
        release(&ptable.lock);
        return 0;
      }
      isThereThd = 1;
      if(thd->state == ZOMBIE){/*after that thread exit*/
          // Found one.
        //cprintf("join done ticks:%d  pid: %d, thread: %d, tid: %d , sz: %d, thdva: %d\n", ticks, thd->pid, thread, thd->tid, thd->sz, thd->thdva);
        kfree(thd->kstack);
        thd->kstack = 0;
        thd->pid = 0;
        thd->parent = 0;
        thd->name[0] = 0;
        thd->killed = 0;
          /*for project02*/
        thd->mem_limit = 0;
        thd->sPage_cnt = 0;
        //thd-> tid = 0;
        *retval = thd->retval;
        thd->retval = 0;

        uint tmpva= thd->thdva;
        thd->thdva = 0;
        
        deallocuvm(thd->pgdir, tmpva + 2*PGSIZE , tmpva);
        curprocPar->spare[curprocPar->rear] = tmpva;
        curprocPar->rear = (curprocPar->rear+1) % NTHREAD;
        thd->state = UNUSED;

        release(&ptable.lock);
        return 0;
      }
    }
    if(!isThereThd||curproc->killed){
      release(&ptable.lock);
      return -1;
    }

    //cprintf("join SLEEPING ticks:%d wanna find thread %d\n", ticks, thread);
    //cprintf("join gonnabe sleep ticks: %d\n", ticks);
    sleep((void*)thread, &ptable.lock);
    //cprintf("dd\n");
  }
}

/********************************
            project02
  clean thread without caller
*********************************/
void
thread_cleaner(Thread *curthd){
  Thread *thd;
  int tmp;
  acquire(&ptable.lock);
  for(thd = ptable.proc; thd < &ptable.proc[NPROC];thd++){
    if(thd->pid == curthd->pid && thd != curthd){
     // uint tmpva = thd->thdva;
     // struct proc *tmpPar = thd->tmaker;
      tmp = thd->tid;
      kfree(thd->kstack);
      thd->kstack = 0;
      thd->pid = 0;
      thd->parent = 0;
      thd->name[0] = 0;
      thd->killed = 0;
      thd->mem_limit = 0;
      thd->sPage_cnt = 0;
      //thd-> tid = 0;
      thd->retval = 0;
      thd->thdva = 0;
      thd->sz = 0;
      thd->state = UNUSED;
      wakeup1((void *)tmp);

    }
  }
  release(&ptable.lock);
}

/********************************
            project02
  make sz of threds family same
*********************************/
int 
thread_family_sz_same(int pid, uint sz){
  Thread *thd;
  acquire(&ptable.lock);
  for(thd=ptable.proc;thd<&ptable.proc[NPROC];thd++){
    if(thd->pid == pid){
      thd->sz = sz;
    }
  }
  release(&ptable.lock);
  return 0;
}