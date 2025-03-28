#include "types.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "mmu.h"
#include "x86.h"
#include "proc.h"
#include "spinlock.h"
#include "mlfq.h"

struct {
  struct spinlock lock;
  struct proc *lockedProc;
  struct proc proc[NPROC];
} ptable;

//L0 queue
Queue q0 = {0, 0, {0, }};
//L1 queue
Queue q1 = {0, 0, {0, }};
//L2 queue
pQueue q2 = {0, {0, }};
//mlfq
Mlfq mlfq = {0, &q0, &q1, &q2};



static struct proc *initproc;

int nextpid = 1;
extern void forkret(void);
extern void trapret(void);

static void wakeup1(void *chan);



//ptable lock
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
  
  p->qLevel = 0;
  p->priority = 3;
  p->runTime= 0;
  p->isSchedLocked = 0;
  p->l2Time = 0;
  
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


  enQueue(mlfq.L0, p);
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
  /*
  p->qLevel = 0;
  p->priority = 3;
  p->runTime= 0;
  p->isSchedLocked = 0;
  enQueue(mlfq.L0, p);
  */
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

  sz = curproc->sz;
  if(n > 0){
    if((sz = allocuvm(curproc->pgdir, sz, sz + n)) == 0)
      return -1;
  } else if(n < 0){
    if((sz = deallocuvm(curproc->pgdir, sz, sz + n)) == 0)
      return -1;
  }
  curproc->sz = sz;
  switchuvm(curproc);
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
  struct proc *curproc = myproc();

  // Allocate process.
  if((np = allocproc()) == 0){
    return -1;
  }

  // Copy process state from proc.
  if((np->pgdir = copyuvm(curproc->pgdir, curproc->sz)) == 0){
    kfree(np->kstack);
    np->kstack = 0;
    np->state = UNUSED;
    return -1;
  }
  np->sz = curproc->sz;
  np->parent = curproc;
  *np->tf = *curproc->tf;

  // Clear %eax so that fork returns 0 in the child.
  np->tf->eax = 0;

  for(i = 0; i < NOFILE; i++)
    if(curproc->ofile[i])
      np->ofile[i] = filedup(curproc->ofile[i]);
  np->cwd = idup(curproc->cwd);

  safestrcpy(np->name, curproc->name, sizeof(curproc->name));

  pid = np->pid;
  /*
  np->qLevel = 0;
  np->priority = 3;
  np->runTime= 0;
  np->isSchedLocked = 0;
  enQueue(mlfq.L0, np);
  */

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
  struct proc *curproc = myproc();
  
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
        kfree(p->kstack);
        p->kstack = 0;
        freevm(p->pgdir);
        p->pid = 0;
        p->parent = 0;
        p->name[0] = 0;
        p->killed = 0;
        p->state = UNUSED;
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
  struct cpu *c = mycpu();
  c->proc = 0;
  for(;;){
    // Enable interrupts on this processor.
    sti();
    // Loop over process table looking for process to run.
    acquire(&ptable.lock);
    /**************
     SCHEUDLER LOCK
    ***************/ 
    if(ptable.lockedProc){
      if(ptable.lockedProc->state == ZOMBIE){
        cprintf("the state of locked process is zombie! sched Unlock\n");
        schedulerUnlock(2019082279);
      }else{
        if(ptable.lockedProc->state == RUNNABLE){
          c->proc = ptable.lockedProc;
          switchuvm(ptable.lockedProc);
          ptable.lockedProc->state = RUNNING;
          swtch(&(c->scheduler), ptable.lockedProc->context);
          switchkvm();
          c->proc = 0;
          //if(mlfq.gTicks == 55) schedulerUnlock(2019082279); for schedulerUnlock test!
          if(ptable.lockedProc){
            cprintf("ticks: %d, pid: %d, isLock: %d\n", mlfq.gTicks, ptable.lockedProc->pid, 
                    ptable.lockedProc->isSchedLocked);
          }
        }
      }
      release(&ptable.lock);
      continue;
    }
    /**************
    MLFQ SCHEDULING
    ***************/ 
    struct proc *p;
    //Is there RUNNABLE pro in L0
    if(queue_isThereRunnable(mlfq.L0))//MLFQ_L0
    {
      p = deQueue(mlfq.L0);
      if(p && p->state == RUNNABLE)
      {
        c->proc = p;
        switchuvm(p);
        p->state = RUNNING;
        swtch(&(c->scheduler), p->context);
        switchkvm();
        c->proc = 0;
        //cprintf("%d L0 pid: %d, qlev: %d, runTime: %d , ticks: %d, state: %d\n",mlfq.gTicks, p->pid, p->qLevel, p->runTime, ticks, p->state);
      }
      if(p->isSchedLocked){
        ptable.lockedProc = p;
      }else{
        if(p && p->qLevel == 0&&p->state!= ZOMBIE)
        { 
          enQueue(mlfq.L0, p);
        }
      }
    }
    else//mlfq.L0 is empty
    {
      if(queue_isThereRunnable(mlfq.L1))//MLFQ_L1
      {
        p =deQueue(mlfq.L1);
        if(p && p->state == RUNNABLE )
        {
          c->proc = p;
          switchuvm(p);
          p->state = RUNNING;
          swtch(&(c->scheduler), p->context);
          switchkvm();
          c->proc = 0;
          //cprintf("%d L1 pid: %d ql: %d, pr prior : %d, rTime: %d\n",mlfq.gTicks, p->pid, p->qLevel, p->priority, p->runTime);
        }
        if(p->isSchedLocked){
          ptable.lockedProc = p;
        }else{
          if(p && p->qLevel == 1&& p->state != ZOMBIE)
          {
            enQueue(mlfq.L1, p);
          }
        }
      }
      else//mlfq.L1 is empty
      {
        if(mlfq.L2->count)//MLFQ_L2
        {
          p =pq_deQueue(mlfq.L2);
          if(p->state == RUNNABLE)
          {
            c->proc = p;
            switchuvm(p);
            p->state = RUNNING;
            //cprintf("%d state : %d count : %d , A L2 pid :%d ql: %d, pd prior : %d, rTime: %d, L2time: %d, ticks: %d\n",mlfq.gTicks, p->state, mlfq.L2->count, p->pid, p->qLevel, p->priority, p->runTime, p->l2Time, ticks);
            swtch(&(c->scheduler), p->context);
            switchkvm();
            c->proc = 0;
            //cprintf("%d state : %d count : %d , B L2 pid :%d ql: %d, pd prior : %d, rTime: %d, L2time: %d, ticks: %d\n",mlfq.gTicks, p->state, mlfq.L2->count,  p->pid, p->qLevel, p->priority, p->runTime, p->l2Time, ticks);
          }
          if(p->isSchedLocked){
            ptable.lockedProc = p;
          }else{
            if(p && p->qLevel == 2&& p->state != ZOMBIE)
            {
              //cprintf("%d state : %d, count : %d , C L2 pid :%d ql: %d, pd prior : %d, rTime: %d, L2time: %d, ticks: %d\n",mlfq.gTicks, p->state, mlfq.L2->count,  p->pid, p->qLevel, p->priority, p->runTime, p->l2Time, ticks);
              pq_enQueue(mlfq.L2, p);
            }
          }
        }
      }
    }
    release(&ptable.lock);
  }//for(;;)end
}//schedular func end   

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
  // cprintf("%d in sched %d ql: %d\n",mlfq.gTicks, myproc()->pid, myproc()->qLevel);
  swtch(&p->context, mycpu()->scheduler);
  mycpu()->intena = intena;
}

// Give up the CPU for one scheduling round.
void
yield(void)
{
  acquire(&ptable.lock);  //DOC: yieldlock
  int pbflag = 0;
  if(mlfq.gTicks == 100){
    pbflag = 1;
    priorityBoosting();
  }
  mlfq.gTicks++;
  if(!pbflag && !(myproc()->isSchedLocked)){
    struct proc *p = myproc();
    switch(p->qLevel){
      case 0:
        if(p->runTime < 4){
          p->runTime++;
          break;
        }
        if(p->runTime >= 4){
          queue_timeQuantumOverflow_toQ(myproc());
        }
        break;
      case 1:
        if(p->runTime < 6){
          p->runTime++;
          break;
        }
        if(p->runTime >= 6){
          queue_timeQuantumOverflow_toPQ(myproc());
        }
        break;
      case 2:
        if(p->runTime < 8){
          p->runTime++;
          break;
        }
        if(p->runTime >=8){
          pq_timeQuantumOverflow(myproc());
        }
        break;
    }
  }
  myproc()->state = RUNNABLE;
  //cprintf("%d before sched %d ql: %d\n",mlfq.gTicks, myproc()->pid, myproc()->qLevel);
  sched();
  release(&ptable.lock);
}

void 
priorityBoosting(void)
{
  if(myproc()->isSchedLocked){
    schedulerUnlock(2019082279);
  }
  mlfq.gTicks = 0;
  myproc()->qLevel = 0;
  myproc()->priority = 3;
  myproc()->runTime = 0;
  myproc()->l2Time = 0;
  enQueue(mlfq.L0, myproc());
  queue_moveAllProcAtoB(mlfq.L0, mlfq.L0);
  queue_moveAllProcAtoB(mlfq.L1, mlfq.L0);
  pq_moveAllProcAtoQueueB(mlfq.L2,mlfq.L0, myproc()->pid);
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

  acquire(&ptable.lock);
  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
    if(p->pid == pid){
      p->killed = 1;
      // Wake process from sleep if necessary.
      if(p->state == SLEEPING)
        p->state = RUNNABLE;
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

//get myproc queue level
int 
getLevel(void)
{
  return (myproc()->qLevel);
}

void 
setPriority(int pid, int priority)
{
  acquire(&ptable.lock);
  struct proc* pr;
  for(pr = ptable.proc; pr < &ptable.proc[NPROC];pr++){
    if(pr && pr->pid == pid){
      pr->priority = priority;
      break;
    }
  }
  release(&ptable.lock);
}

void
schedulerLock(int password)
{
  struct proc *myP = myproc();
  if(myproc()->state != RUNNING){
    cprintf("this process is not running! only RUNNING process can be schedulerLocked\n");
    return;
  }
  if(password != 2019082279){
    cprintf("schedulerLock fail: pwd uncorrect! pid: %d, time quantum: %d, queue level: %d\n", myP->pid, myP->runTime, myP->qLevel);
    myP->isSchedLocked = 0;
    kill(myP->pid);
  }
  else//password correcet
  {
    if(!myP->isSchedLocked)
    {
      mlfq.gTicks = 0;
      myP->isSchedLocked = 1;
      ptable.lockedProc = myP;
      cprintf("setting scheduler Lock\n");
    }
    else{
      cprintf("schedulerLock fail: process is already in schedulerLock\n");
    }
  }
}//schedularLock func end

void
schedulerUnlock(int password)
{
  struct proc *myP;
  if(ptable.lockedProc){//to schedulerUnlock locked process 
    myP = ptable.lockedProc;
  }else{//to schedulerUnlock RUNNING process
    myP = myproc();
  }
  if(password != 2019082279){
    cprintf("schedulerUnlock fail: pwd uncorrect! pid: %d, time quantum: %d, queue level: %d\n", myP->pid, myP->runTime, myP->qLevel);
    myP->isSchedLocked = 0;
    kill(myP->pid);
  }
  else//password correcet
  {
    if(myP->isSchedLocked)
    {
      myP->runTime = 0;
      myP->priority = 3;
      myP->isSchedLocked = 0;
      ptable.lockedProc = 0;
      enQueue(mlfq.L0, myP);
      cprintf("sched unlock clear\n");
    }
    else{
      cprintf("schedulerUnLock fail: this process %d is not in schedulerLock!!\n", myP->pid );
    }
  }
}//schedularUnlock func end
