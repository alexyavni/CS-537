#include "types.h"
#include "stat.h"
#include "user.h"

int
main(int argc, char **argv)
{
  int i;

  if(argc < 1){
    printf(2, "usage: get file num...\n");
    exit();
  }
  for(i=1; i<argc; i++)
    getfilenum(atoi(argv[i]));
  exit();
}
