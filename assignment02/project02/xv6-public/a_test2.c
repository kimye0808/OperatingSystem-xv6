
 

#include "types.h"
#include "stat.h"
#include "user.h"

#define NUM_THREAD 10
#define NTEST 5

volatile int gcnt;

// Test fork system call in multi-threaded environment
int forktest(void);

// Test what happen when threads requests sbrk concurrently
int sbrktest(void);

// Test what happen when threads kill itself
int killtest(void);

// Test pipe is worked in multi-threaded environment
int pipetest(void);

// Test sleep system call in multi-threaded environment
int sleeptest(void);

int gpipe[2];

int (*testfunc[NTEST])(void) = {
  forktest,
  sbrktest,
  killtest,
  pipetest,
  sleeptest,
};
char *testname[NTEST] = {
  "forktest",
  "sbrktest",
  "killtest",
  "pipetest",
  "sleeptest",
};

int
main(int argc, char *argv[])
{
  int i;
  int ret;
  int pid;
  int start = 0;
  int end = NTEST-1;
  if (argc >= 2)
    start = atoi(argv[1]);
  if (argc >= 3)
    end = atoi(argv[2]);

  for (i = start; i <= end; i++){
    printf(1,"%d. %s start\n", i, testname[i]);
    if (pipe(gpipe) < 0){
      printf(1,"pipe panic\n");
      exit();
    }
    ret = 0;

    if ((pid = fork()) < 0){
      printf(1,"fork panic\n");
      exit();
    }
    if (pid == 0){
      close(gpipe[0]);
      ret = testfunc[i]();
      write(gpipe[1], (char*)&ret, sizeof(ret));
      close(gpipe[1]);
      exit();
    } else{
      close(gpipe[1]);
      if (wait() == -1 || read(gpipe[0], (char*)&ret, sizeof(ret)) == -1 || ret != 0){
        printf(1,"%d. %s panic\n", i, testname[i]);
        exit();
      }
      close(gpipe[0]);
    }
    printf(1,"%d. %s finish\n", i, testname[i]);
    sleep(100);
  }
  exit();
}



// ============================================================================

void*
forkthreadmain(void *arg)
{
  int pid;
  if ((pid = fork()) == -1){
    printf(1, "panic at fork in forktest\n");
    exit();
  } else if (pid == 0){
    printf(1, "child\n");
    exit();
  } else{
    printf(1, "parent\n");
    if (wait() == -1){
      printf(1, "panic at wait in forktest\n");
      exit();
    }
  }
  thread_exit(0);

  return 0;
}

int
forktest(void)
{
  thread_t threads[NUM_THREAD];
  int i;
  void *retval;

  for (i = 0; i < NUM_THREAD; i++){
    if (thread_create(&threads[i], forkthreadmain, (void*)0) != 0){
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
  return 0;
}
// ============================================================================


void*
sbrkthreadmain(void *arg)
{
  int tid = (int)arg;
  char *oldbrk;
  char *end;
  char *c;
  oldbrk = sbrk(1000);
  end = oldbrk + 1000;
  for (c = oldbrk; c < end; c++){
    *c = tid+1;
  }
  sleep(1);
  for (c = oldbrk; c < end; c++){
    if (*c != tid+1){
      printf(1, "panic at sbrkthreadmain\n");
      exit();
    }
  }
  thread_exit(0);

  return 0;
}

int
sbrktest(void)
{
  thread_t threads[NUM_THREAD];
  int i;
  void *retval;

  for (i = 0; i < NUM_THREAD; i++){
    if (thread_create(&threads[i], sbrkthreadmain, (void*)i) != 0){
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

  return 0;
}



// ============================================================================

void*
killthreadmain(void *arg)
{
  kill(getpid());
  while(1){
    printf(1,"pid: %d\n", getpid());
  };
}

int
killtest(void)
{
  thread_t threads[NUM_THREAD];
  int i;
  void *retval;

  for (i = 0; i < NUM_THREAD; i++){
    if (thread_create(&threads[i], killthreadmain, (void*)i) != 0){
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
  //while(1);
  return 0;
}

// ============================================================================

void*
pipethreadmain(void *arg)
{
  int type = ((int*)arg)[0];
  int *fd = (int*)arg+1;
  int i;
  int input;
  for (i = -5; i <= 5; i++){
    if (type){
      read(fd[0], &input, sizeof(int));
      __sync_fetch_and_add(&gcnt, input);
      //gcnt += input;
    } else{
      write(fd[1], &i, sizeof(int));
    }
  }
  thread_exit(0);

  return 0;
}

int
pipetest(void)
{
  thread_t threads[NUM_THREAD];
  int arg[3];
  int fd[2];
  int i;
  void *retval;
  int pid;

  if (pipe(fd) < 0){
    printf(1, "panic at pipe in pipetest\n");
    return -1;
  }
  arg[1] = fd[0];
  arg[2] = fd[1];
  if ((pid = fork()) < 0){
      printf(1, "panic at fork in pipetest\n");
      return -1;
  } else if (pid == 0){
    close(fd[0]);
    arg[0] = 0;
    for (i = 0; i < NUM_THREAD; i++){
      if (thread_create(&threads[i], pipethreadmain, (void*)arg) != 0){
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
    close(fd[1]);
    exit();
  } else{
    close(fd[1]);
    arg[0] = 1;
    gcnt = 0;
    for (i = 0; i < NUM_THREAD; i++){
      if (thread_create(&threads[i], pipethreadmain, (void*)arg) != 0){
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
    close(fd[0]);
  }
  if (wait() == -1){
    printf(1, "panic at wait in pipetest\n");
    return -1;
  }
  if (gcnt != 0)
    printf(1,"panic at validation in pipetest : %d\n", gcnt);

  return 0;
}

// ============================================================================

void*
sleepthreadmain(void *arg)
{
  sleep(1000000);
  thread_exit(0);

  return 0;
}

int
sleeptest(void)
{
  thread_t threads[NUM_THREAD];
  int i;

  for (i = 0; i < NUM_THREAD; i++){
    if (thread_create(&threads[i], sleepthreadmain, (void*)i) != 0){
        printf(1, "panic at thread_create\n");
        return -1;
    }
  }
  sleep(10);
  return 0;
}


//////////////////////////////////////////////////////////////////////////////////////////////