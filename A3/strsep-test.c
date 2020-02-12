#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main () {
  char* str = strdup("ls     -l");
  char* str_backup = str;
  // strsep does not work on constant strings
  
  printf("str is at %p\n", str);
  char* p = strsep(&str, " ");
  printf("str is at %p p is at %p\n", str, p);
  while (p != NULL) {
    if (strlen(p) > 0) {
      printf("%s\n", p);
    }
    p = strsep(&str, " ");
    // printf("str is at %p p is at %p\n", str, p);
  }

  free(str_backup);
  return 0;
}
