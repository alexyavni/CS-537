#include <stdio.h>
#include <stdlib.h>

extern void xv6_srand(unsigned int);
extern int xv6_rand();

// wrapper function to overwrite stdlib rand function
int __wrap_rand (void)
{
    return 0;
}


int main(int argc, char * argv[]) {

    xv6_srand((unsigned int)atoi(argv[1]));
    int range = atoi(argv[2]);

    for (size_t i = 0; i < 100000; i ++) {
        printf("%d ",  xv6_rand() % range);
    }

}