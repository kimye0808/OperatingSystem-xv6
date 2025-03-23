#include "types.h"
#include "stat.h"
#include "user.h"

#define NUM_LOOP 1000000
#define NUM_PROCS 6

int main(int argc, char *argv[])
{
  int pid, i;
  int start_ticks, end_ticks;
  int parent = getpid();

  printf(1, "\nRound Robin Test Start\n");

  for (i = 0; i < NUM_PROCS; i++) {
    if ((pid = fork()) == 0) {
      int cnt = 0;
      start_ticks = uptime();
      for (int j = 0; j < NUM_LOOP; j++) {
        cnt++;
      }
      end_ticks = uptime();

      printf(1, "Process %d completed in %d ticks\n", getpid(), end_ticks - start_ticks);
      exit();
    }
  }

  if (getpid() == parent) {
    while (wait() != -1);  // Wait for all children to finish
  }

  printf(1, "Round Robin Test End\n");
  exit();
}
