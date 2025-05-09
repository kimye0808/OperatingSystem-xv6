#include "types.h"
#include "stat.h"
#include "user.h"

int
main(int argc, char *argv[])
{
  if(argc != 4){
    printf(2, "Usage1 hard link: ln -h old new | Usage2 symbolic link: ln -s old new\n");
    exit();
  }
  if(!strcmp(argv[1], "-h" )){
    if(link(argv[2], argv[3]) < 0){
      printf(2, "link %s %s: failed\n", argv[2], argv[3]);
    }
  }else if(!strcmp(argv[1], "-s")){
    if(symLink(argv[2], argv[3]) < 0){
      printf(2, "symbolic link %s %s: failed\n", argv[2], argv[3]);
    }
  }else{
    printf(2, "Usage1 hard link: ln -h old new | Usage2 symbolic link: ln -s old new\n");
  }
  exit();
}
