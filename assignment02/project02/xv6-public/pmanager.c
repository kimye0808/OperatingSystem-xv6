#include "types.h"
#include "user.h"
#include "fcntl.h"


#define MAXARGS 32

int
getcmd(char *buf, int nbuf)//-sh.c
{
  memset(buf, 0, nbuf);
  gets(buf, nbuf);
  if(buf[0] == 0) // EOF
    return -1;
  return 0;
}

int 
main(void)
{
  static char buf[100];
  int fd;

  // Ensure that three file descriptors are open. - sh.c
  while((fd = open("console", O_RDWR)) >= 0){
    if(fd >= 3){
      close(fd);
      break;
    }
  }
  
  // following mannual
  printf(1, "\n");
  printf(1, "[You must follow this format]\n");
  printf(1, "a.   exit\n");
  printf(1, "b.   list\n");
  printf(1, "c.   kill <pid>\n");
  printf(1, "d.   execute <path> <stacksize> (stacksize [1,100])\n");
  printf(1, "e.   memlim  <pid> <limit>\n");
  printf(1, "\n");

  // Read and run input commands.
  while(getcmd(buf, sizeof(buf)) >= 0){
    if(buf[0] == 'e' && buf[1] == 'x' && buf[2] == 'i' && buf[3] == 't' && /*exit*/
            buf[4] == '\n' )
    {
      exit();
    }
    else if(buf[0] == 'l' && buf[1] == 'i' && buf[2] == 's' && buf[3] == 't' && /*list*/
      buf[4] == '\n')
    {
      listProc();
    }
    else if(buf[0] == 'k'&& buf[1] == 'i' && buf[2] == 'l' && buf[3] == 'l' &&  /*kill <pid>*/
            buf[4] == ' ')
    {
      int pid = 0;
      int buf_idx = 5;
      int option_digit = 0;
      int option_not_valid = 0;
      /*pid scan*/
      while(buf[buf_idx] != ' ' && buf[buf_idx] != '\n'){
        pid *= 10;
        int adder = buf[buf_idx] - '0';
        //printf(2, "adder: %d\n", adder);
        if(adder < 0 || adder > 9){/*option each number of digit check*/
          option_not_valid = 1;
          break;
        }
        if(++option_digit > 9){/*option digit check 000,000,000*/
          option_not_valid = 1;
          break;
        }
        pid += adder;
        buf_idx++;
      }
      /*option valid check*/
      if(option_not_valid){
        printf(2, "given input is not valid: only kill <pid> and option >= 0 or <= a billion\n");
        continue;
      }
      /*normal kill*/
      if(kill(pid) == - 1){
        printf(2, "kill fail\n");
      }else{
        printf(1, "kill success\n");
      }
    }
    else if(buf[0] == 'e' && buf[1] == 'x' && buf[2] == 'e' && buf[3] == 'c' && buf[4] == 'u' && buf[5] == 't' && buf[6] == 'e' && /*execute <path> <stack size>*/
            buf[7] == ' ')          
    {
      char path[50];
      int stacksize = 0; int buf_idx = 8; int option_idx = 0; int option_not_valid = 0;
      char *argv[MAXARGS];
      /*path scan*/
      while(buf[buf_idx] != ' ' && buf[buf_idx] != '\n'){
        if(option_idx>=50){//0~49
          option_not_valid = 1;
          break;
        }
        path[option_idx] = buf[buf_idx];
        option_idx++;
        buf_idx++;
      }
      /*blank space*/
      option_idx = 0;
      buf_idx++;
      /*stack size scan*/
      while(buf[buf_idx] != ' ' && buf[buf_idx] != '\n'){
        stacksize *= 10;
        int adder = buf[buf_idx] - '0';
        if(adder < 0 || adder > 9){/*option each number of digit check*/
          option_not_valid = 1;
          break;
        }
        if(++option_idx > 9){/*option digit check 000,000,000*/
          option_not_valid = 1;
          break;
        }
        stacksize += adder;
        buf_idx++;
      }
      /*option not valid*/
      if(option_not_valid){
        printf(2, "given input is not valid: only execute <path> <stack size> and each option >=0 or <= a billion\n");
        continue;
      }
      argv[0] = path;
      /*normal execute*/
      int pid = fork();
      //printf(1, "pid %d, stacksize: %d, path %s\n", pid, stacksize, path);
      if(!pid){//for child pmanager
        int cpid = fork();
        if(!cpid){
          if(exec2(path, argv, stacksize) == -1){/*if child && execute error*/
            printf(2, "execute fail\n");
          }
        }else if(cpid>0){
          wait();
        }else if(cpid== 1){
          printf(2, "fork fail\n");/*fork fail*/
        }
        exit();
      }else if(pid == -1) {
        printf(2, "fork fail\n");/*fork fail*/
      }else if(pid > 0){//parent pmanager
        continue;
      }
    }
    else if(buf[0] == 'm' && buf[1] == 'e' && buf[2] == 'm' && buf[3] == 'l' && buf[4] == 'i' && buf[5] == 'm' && /*memlimit <pid> <limit>*/
            buf[6] == ' ')
    {
      int pid = 0; int limit = 0;
      int buf_idx = 7;
      int option_digit = 0;
      int option_not_valid = 0;
      /*pid scan*/
      while(buf[buf_idx] != ' ' && buf[buf_idx] != '\n'){
        pid *= 10;
        int adder = buf[buf_idx] - '0';
        if(adder < 0 || adder > 9){/*option each number of digit check*/
          option_not_valid = 1;
          break;
        }
        if(++option_digit > 9){/*option digit check 000,000,000*/
          option_not_valid = 1;
          break;
        }
        pid += adder;
        buf_idx++;
      }
      /*blank space*/
      option_digit = 0;
      buf_idx++;
      /*limit scan*/
      while(buf[buf_idx] != ' ' && buf[buf_idx] != '\n'){
        limit *= 10;
        int adder = buf[buf_idx] - '0';
        if(adder < 0 || adder > 9){/*option each number of digit check*/
          option_not_valid = 1;
          break;
        }
        if(++option_digit > 9){/*option digit check 000.000.000*/
          option_not_valid = 1;
          break;
        }
        limit += adder;
        buf_idx++;
      }
      /*handle option error*/
      if(option_not_valid){
        printf(2, "given input is not valid: only memlim <pid> <limit> and each option >=0 or <= a billion\n");
        continue;
      }
      /*normal set mem limit*/
      if(setmemorylimit(pid, limit)== -1){
        printf(2, "setmemorylimit fail\n");
      }else{
        printf(1, "setmemorylimit success\n");
      }
    }
    else{
      printf(2, "given input is not valid: only 'exit', 'list', 'kill <pid>', 'execute <path> <stack size>', 'memlim <pid> <limit>'\n");
    }//case end
    printf(1, "\n");
  }//while getcmd end
}