#include "types.h"
#include "stat.h"
#include "user.h"

#define NUM_CHILD 5
#define NUM_LOOP1 50000
#define NUM_LOOP2 300000
#define NUM_LOOP3 200000
#define NUM_LOOP4 500000

int child;

// ret: 0: child, 1: parent
int create_child(void) {
  for (int i = 0; i < NUM_CHILD; i++) {
    int pid = fork();
    if (pid == 0) {
      child = i;
      sleep(10);
      return 0;
    }
  }
  return 1;
}

void exit_child(int parent) {
  if (parent)
    while (wait() != -1); // wait for all child processes to finish
  else
    exit();
}

int main(int argc, char **argv) {
  int p; // is this parent?

  printf(1, "MLFQ test start\n");
/*

  // Test 1
  printf(1, "\nFocused priority\n");

  p = create_child();

  if (!p) {
    int pid = getpid();
    int cnt[3] = {0};
    for (int i = 0; i < NUM_LOOP1; i++) {
      cnt[getLevel()]++;
      setPriority(pid, child * 2);
    }
    printf(1, "process %d: L0=%d, L1=%d L2=%d\n", pid, cnt[0], cnt[1], cnt[2]);
  }

  exit_child(p);

  // Test 2
  printf(1, "\nWithout priority manipulation\n");

  p = create_child();

  if (!p) {
    int pid = getpid();
    int cnt[3] = {0};
    for (int i = 0; i < NUM_LOOP2; i++)
      cnt[getLevel()]++;
    printf(1, "process %d: L0=%d, L1=%d, L2=%d\n", pid, cnt[0], cnt[1], cnt[2]);
  }

  exit_child(p);

  // Test 3
  printf(1, "\nWith yield\n");

  p = create_child();

  if (!p) {
    int pid = getpid();
    int cnt[3] = {0};
    for (int i = 0; i < NUM_LOOP3; i++) {
      cnt[getLevel()]++;
      yield();
    }
    printf(1, "process %d: L0=%d, L1=%d, L2=%d\n", pid, cnt[0], cnt[1], cnt[2]);
  }

  exit_child(p);
  */
  // Test 4
  printf(1, "\nschedulerLock\n");

  p = create_child();

  if (!p) {
    int pid = getpid();
    int cnt[3] = {0};
    if (child == NUM_CHILD - 3)
      schedulerLock(2019082279);
    
    for (int i = 0; i < NUM_LOOP4; i++){
      cnt[getLevel()]++;
    }
    
    //if (child == NUM_CHILD - 3) schedulerLock(201908227);
   
    printf(1, "process %d: L0=%d, L1=%d, L2=%d\n", pid, cnt[0], cnt[1], cnt[2]);

    if (child == NUM_CHILD - 3) schedulerUnlock(2019082279);
  }  

  exit_child(p);
  exit();
}