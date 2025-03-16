// Mutual exclusion spin locks.

#include "types.h"
#include "defs.h"
#include "param.h"
#include "x86.h"
#include "memlayout.h"
#include "mmu.h"
#include "proc.h"
#include "spinlock.h"

void
initlock(struct spinlock *lk, char *name)
{
  lk->name = name;
  lk->locked = 0;
  lk->cpu = 0;
}

/*xv6의 spinlock은 공유자원에 대한 접근을 동시에 발생하지 않도록 하는 락입니다. spinlock은 busy-waiting 기반으로 구현되어 있어 다른 프로세스가 lock을 해제할 때까지 무한히 기다리게 됩니다.
acquire 함수는 spinlock을 획득하는 함수입니다. pushcli 함수를 호출하여 인터럽트를 비활성화한 다음, lk가 이미 다른 프로세스에 의해 점유되어 있지 않은 경우까지 busy-waiting을 합니다.
 lk가 획득되면, __sync_synchronize 함수를 호출하여 메모리 내용의 일관성을 보장하고, 현재 cpu와 실행중인 함수의 pc를 spinlock 구조체에 저장합니다.
 
release 함수는 spinlock을 해제하는 함수입니다. holding 함수를 사용하여 현재 프로세스가 해당 spinlock을 보유하고 있는지 확인한 다음, __sync_synchronize 함수를 호출하여 메모리 내용의 일관성을 보장합니다. 
마지막으로, asm volatile 구문을 사용하여 lk->locked에 0을 할당하여 spinlock을 해제합니다. 마지막으로, popcli 함수를 사용하여 인터럽트를 다시 활성화합니다.*/

/*xchg 함수는 인자로 전달된 값을 원자적으로 교환하는 기계어 명령어입니다. 이 함수는 인자로 전달된 변수와 0 사이의 값을 교환합니다. 이 때 변수는 메모리 어드레스가 될 수도 있고 레지스터가 될 수도 있습니다.
xchg 함수는 현재 값을 읽은 후에 새로운 값을 변수에 저장하고, 이전 값을 리턴합니다. 이러한 원자적 교환 연산은 다중 스레드 프로그래밍 환경에서 공유된 자원에 대한 경쟁 조건을 해결하기 위해 사용됩니다. 
spinlock에서는 락을 얻기 위해 xchg 함수를 사용하여 해당 lock 변수의 값을 원자적으로 1로 설정하고, 이미 lock이 획득된 상태라면 다른 스레드가 lock을 기다리도록 합니다. 반대로 락을 해제하기 위해서도 xchg 함수를 사용합니다.
원자성(atomicity)은 중간에 다른 스레드나 인터럽트 등에 의해 접근되거나 변경되지 않는 것을 말합니다. 이 경우 다른 스레드나 인터럽트 등이 동시에 같은 변수에 접근하더라도,
 xchg 함수는 다른 명령어와 함께 실행되는 것이 아니라 딱 한번 실행되어 lock 변수를 1로 설정하게 됩니다. 이렇게 하면 여러 스레드나 인터럽트 등이 lock 변수를 동시에 1로 설정하는 것을 막을 수 있으며, 이를 통해 상호배제(mutex)를 구현할 수 있습니다.
*/
// Acquire the lock.
// Loops (spins) until the lock is acquired.
// Holding a lock for a long time may cause
// other CPUs to waste time spinning to acquire it.
void
acquire(struct spinlock *lk)
{
  pushcli(); // disable interrupts to avoid deadlock.
  if(holding(lk))
    panic("acquire");

  // The xchg is atomic.
  while(xchg(&lk->locked, 1) != 0)
    ;

  // Tell the C compiler and the processor to not move loads or stores
  // past this point, to ensure that the critical section's memory
  // references happen after the lock is acquired.
  __sync_synchronize();

  // Record info about lock acquisition for debugging.
  lk->cpu = mycpu();
  getcallerpcs(&lk, lk->pcs);
}

// Release the lock.
void
release(struct spinlock *lk)
{
  if(!holding(lk))
    panic("release");

  lk->pcs[0] = 0;
  lk->cpu = 0;

  // Tell the C compiler and the processor to not move loads or stores
  // past this point, to ensure that all the stores in the critical
  // section are visible to other cores before the lock is released.
  // Both the C compiler and the hardware may re-order loads and
  // stores; __sync_synchronize() tells them both not to.
  __sync_synchronize();

  // Release the lock, equivalent to lk->locked = 0.
  // This code can't use a C assignment, since it might
  // not be atomic. A real OS would use C atomics here.
  asm volatile("movl $0, %0" : "+m" (lk->locked) : );

  popcli();
}

// Record the current call stack in pcs[] by following the %ebp chain.
void
getcallerpcs(void *v, uint pcs[])
{
  uint *ebp;
  int i;

  ebp = (uint*)v - 2;
  for(i = 0; i < 10; i++){
    if(ebp == 0 || ebp < (uint*)KERNBASE || ebp == (uint*)0xffffffff)
      break;
    pcs[i] = ebp[1];     // saved %eip
    ebp = (uint*)ebp[0]; // saved %ebp
  }
  for(; i < 10; i++)
    pcs[i] = 0;
}

// Check whether this cpu is holding the lock.
int
holding(struct spinlock *lock)
{
  int r;
  pushcli();
  r = lock->locked && lock->cpu == mycpu();
  popcli();
  return r;
}


// Pushcli/popcli are like cli/sti except that they are matched:
// it takes two popcli to undo two pushcli.  Also, if interrupts
// are off, then pushcli, popcli leaves them off.

void
pushcli(void)
{
  int eflags;

  eflags = readeflags();
  cli();
  if(mycpu()->ncli == 0)
    mycpu()->intena = eflags & FL_IF;
  mycpu()->ncli += 1;
}

void
popcli(void)
{
  if(readeflags()&FL_IF)
    panic("popcli - interruptible");
  if(--mycpu()->ncli < 0)
    panic("popcli");
  if(mycpu()->ncli == 0 && mycpu()->intena)
    sti();
}

