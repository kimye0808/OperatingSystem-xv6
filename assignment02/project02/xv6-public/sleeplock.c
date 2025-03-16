// Sleeping locks

#include "types.h"
#include "defs.h"
#include "param.h"
#include "x86.h"
#include "memlayout.h"
#include "mmu.h"
#include "proc.h"
#include "spinlock.h"
#include "sleeplock.h"

void
initsleeplock(struct sleeplock *lk, char *name)
{
  initlock(&lk->lk, "sleep lock");
  lk->name = name;
  lk->locked = 0;
  lk->pid = 0;
}

/*
sleeplock은 기본적으로 spinlock과 유사하지만, 차이점이 존재합니다.
 spinlock은 다른 프로세서가 잠금을 획득하기 전까지 대기하며 무한 루프에서 기다리기 때문에, 다른 프로세서가 진행하지 못하고 기다리게 됩니다. 
 하지만 sleeplock은 대기 중인 프로세스를 슬립 상태로 전환시키기 때문에 CPU 자원을 낭비하지 않습니다.
따라서, sleeplock에서는 spinlock과는 달리 무한 루프 대기를 하지 않기 때문에 pushcli나 xchg를 사용하지 않습니다. 
대신에, 대기 중인 프로세스를 슬립 상태로 바꾸고 다른 프로세스가 해당 잠금을 해제하기 전까지 대기하는 것입니다.*/


void
acquiresleep(struct sleeplock *lk)
{
  acquire(&lk->lk);
  while (lk->locked) {
    sleep(lk, &lk->lk);
  }
  lk->locked = 1;
  lk->pid = myproc()->pid;
  release(&lk->lk);
}

void
releasesleep(struct sleeplock *lk)
{
  acquire(&lk->lk);
  lk->locked = 0;
  lk->pid = 0;
  wakeup(lk);
  release(&lk->lk);
}

int
holdingsleep(struct sleeplock *lk)
{
  int r;
  
  acquire(&lk->lk);
  r = lk->locked && (lk->pid == myproc()->pid);
  release(&lk->lk);
  return r;
}



