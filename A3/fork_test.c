#include <sys/types.h>
#include <unistd.h>
#include <stdio.h>

int main(int arg, char** argv) {
	printf("Starting%d\n", getpid());
	int rc = fork();
	if(rc == 0) {
		printf("Exiting from child and PID is %d and RC is%d\n", getpid(), rc);
		char* my_args[3];			// Why 3?
		my_args[0] = "/bin/ls";
		my_args[1] = "-l";
		my_args[2] = NULL;
		int exec_rc = execv("/bin/ls", my_args);
		printf("Done with exec!!!"\n); 		// Never executed: exec never returns here
		// Only way to return would be due to an error
		// We want to wait for the child process to complete
	} else { 	
		// We have this RC which we can use to wait
		int wait_rc = waitpid(rc, NULL, 1);
		printf("Exiting parent process and my PID is %d and RC is %d\n", getpid(), rc);
	}
	return rc;

	// Similar to ls
	// We want to perform ls, return its result, 
	// and wait for it to complete in order to print prompt "smash" again

}
