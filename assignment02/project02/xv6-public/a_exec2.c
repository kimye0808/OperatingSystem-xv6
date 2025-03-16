#include "types.h"
#include "stat.h"
#include "user.h"


#define NUM_THREAD 10
void*
execthreadmain(void *arg)
{
  char *args[3] = {"echo", "echo is executed!", 0}; 
  sleep(1);
  exec("echo", args);

  printf(1, "panic at execthreadmain\n");
  exit();
}

int
main(void)
{
  thread_t threads[NUM_THREAD];
  int i;
  void *retval;

  for (i = 0; i < NUM_THREAD; i++){
    if (thread_create(&threads[i], execthreadmain, (void*)0) != 0){
      printf(1, "panic at thread_create\n");
      return -1;
    }
  }
  for (i = 0; i < NUM_THREAD; i++){
    if (thread_join(threads[i], &retval) != 0){
      printf(1, "panic at thread_join\n");
      return -1;
    }
  }
  printf(1, "panic at exectest\n");
  return 0;
}