#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>

int
main(int argc, char **argv)
{
	if (argc != 2) {
		fprintf(stderr, "usage: %s output_file\n", argv[0]);
		exit(1);
	}
  
	// int newfd = open(argv[1], O_CREAT|O_TRUNC, 0644);
  FILE* new_f = fopen(argv[1], "w");
  int newfd = fileno(new_f);
	printf("This goes to the standard output.\n");

  // FD number 1 is stdout
  // I want to replace it with newfd
  dup2(newfd, 1); 

	printf("This goes to the standard output too in %d.\n", newfd);
	exit(0);
}
