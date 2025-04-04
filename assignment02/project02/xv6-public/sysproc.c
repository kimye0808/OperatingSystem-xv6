#include "types.h"
#include "x86.h"
#include "defs.h"
#include "date.h"
#include "param.h"
#include "memlayout.h"
#include "mmu.h"
#include "proc.h"

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
  return myproc()->pid;
}

int
sys_sbrk(void)
{
  int addr;
  int n;
  
  if(argint(0, &n) < 0)
    return -1;
  addr = myproc()->sz;
  if(growproc(n) < 0){
    return -1;
  }
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
    if(myproc()->killed){
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


/**************************
          project02
process memory limitation
****************************/
int
sys_setmemorylimit(void)
{
  int pid, limit;
  if(argint(0, &pid) < 0 || argint(1, &limit) < 0)
    return -1;
  return setmemorylimit(pid, limit);
}

/********************************
            project02
list of RUNNING & RUNNABLE process
*********************************/
int
sys_listProc(void)
{
  listProc();
  return 0;
}

/********************************
            project02
          create thread
*********************************/
int
sys_thread_create(void)
{
  thread_t thread;
  int start_routine, arg;
  if(argint(0,&thread)<0 || argint(1,&start_routine)<0||argint(2,&arg))
    return -1;
  return thread_create((thread_t*)thread, (void*)start_routine, (void*)arg);
}

/********************************
            project02
           thread exit
*********************************/
int
sys_thread_exit(void)
{
  int retval;
  if(argint(0,&retval)<0)
    return -1;
  thread_exit((void*)retval);
  return 0;
}


/********************************
            project02
          thread  join 
    스레드 생성 프로세스에서 호출
*********************************/
int
sys_thread_join(void)
{
  thread_t thread;
  int retval;
  if(argint(0,&thread)<0||argint(1,&retval)<0)
    return -1;
  return thread_join((thread_t)thread, (void**)retval);
}