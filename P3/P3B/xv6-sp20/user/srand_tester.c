
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#define TEST_LENGTH 10
extern void xv6_srand(unsigned int);
extern int xv6_rand();


// wrapper function to overwrite stdlib rand function
int __wrap_rand (void)
{
    return 0;
}

int main(int argc, char * argv[]) {
    unsigned int t = (unsigned int) time(NULL);
    xv6_srand(t);

    int rst_array[TEST_LENGTH];

    for (size_t i = 0; i < TEST_LENGTH; i++) {
        rst_array[i] = xv6_rand() % 1000;
    }

    xv6_srand(2);
    xv6_rand();
    xv6_rand();
    xv6_rand();

    xv6_srand(t);
    for (size_t i = 0; i < TEST_LENGTH; i++) {
        int rand_num = xv6_rand() % 1000;
        if (rst_array[i] != rand_num) {
            printf("TEST FAILED: got different sequence with the same seed\n");
            exit(1);
        }
    }

    printf("TEST PASSED\n");
    return 0;

}