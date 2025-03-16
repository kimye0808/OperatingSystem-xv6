#include "types.h"
#include "x86.h"
#include "defs.h"
#include "date.h"
#include "param.h"
#include "memlayout.h"
#include "mmu.h"
#include "proc.h"
#include "spinlock.h"

int 
sys_schedulerLock(void)
{
  int password;
  if(argint(0, &password) < 0)
    return -1;
  schedulerLock(password);
  return 0;
}
