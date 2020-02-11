#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

/**********************************************************
 * File name: WIS - UNTAR
 * Author: Alexandra Yavnilovitch
 **********************************************************/
int main (int argc, char *argv[])
{
	/*
	 * Verify that there are enough command line args
	 */
	if(argc < 2)
	{
		printf("wis-untar: tar-file\n");
		exit(1);
	}

	/*
	 * Retrieve the Tar file
	 */
	FILE * file = fopen(argv[1], "r");
	if(file == NULL) 
	{
		printf("wis-untar: cannot open file\n");
                exit(1);
	}

	char filename[100];
	size_t filesize;
	
	/*
	 * Read contents of tar file, and convert into text files
	 */
	while(fread(filename, 1, 100, file))
	{
		FILE * newFile = fopen(filename, "w+");
		fread(&filesize, 8, 1, file);

		char* content = malloc(filesize);
        	fread(content, filesize, 1, file);

		fprintf(newFile, "%s", content);
		fclose(newFile);
		free(content);
	}

	return 0;
}
