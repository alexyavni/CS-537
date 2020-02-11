#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/********************************************************************
 * File name: WIS - GREP
 * Author: Alexandra Yavnilovitch
 ********************************************************************/
int main (int argc, char *argv[])
{
	/*
	 * Verify that there are enough command line args
	 */
	if(argc < 2)
	{
		printf("wis-grep: searchterm [file ...]\n");
		exit(1);
	}

	char* search_term = argv[1];

	// Check if there is a filename
	if (argv[2] == NULL)
	{	
	        char * line = NULL;
	        size_t len = 0;
        	ssize_t read;

		// *fgets(char *s, int size, FILE *stream);		
		while((read = getline(&line, &len, stdin)) != -1) {
                   	char *ret;
                        ret = strstr(line, search_term);
                        if(ret != NULL)
                        {
                                printf("%s", line);
                        }
                }
		exit(0);	
	}

	char* filename = argv[2];
	
	/*
	 * Open file and read lines from file
	 */
	FILE * f = fopen(filename, "r");
	
	/*
	 * Check if the filename exists
	 */
	if(f == NULL) {
		printf("wis-grep: cannot open file\n");
		exit(1);
	}

	char * line = NULL;
	size_t len = 0;
	ssize_t read;
	
	size_t file_index = 2;

	/*
	 * Iterate the file inputs
	 */
	while ( file_index < argc ) {

		/*
		 * Read the lines in the file - check for search string
		 */
		while((read = getline(&line, &len, f)) != -1) {	
			char *ret;
			ret = strstr(line, search_term);
			if(ret != NULL)
			{
				printf("%s", line);
			}

		}	
		
		file_index ++;
		if(file_index < argc) {
		fclose(f);
		filename = argv[file_index];
		line = NULL;
		len = 0;

		f = fopen(filename, "r");
		if(f == NULL) {
			printf("wis-grep: cannot open file\n");
			exit(1);
		}
		}
	}
	fclose(f);

	return 0;
}
