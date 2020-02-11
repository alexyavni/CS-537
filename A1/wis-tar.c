#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

/***************************************************************************
 * File name: WIS - TAR
 * Author: Alexandra Yavnilovitch
 ***************************************************************************/
int main (int argc, char *argv[])
{
	/*
	 * Verify that there are enough command line args
	 */
	if(argc < 3)
	{
		printf("wis-tar: tar-file file […]\n");
		exit(1);
	}

	/*
	 file1 name [100 bytes in ASCII] 
 	 file1 size [8 bytes as binary]
	 contents of file1 [in ASCII]
	 file2 name [100 bytes]
	 file2 size [8 bytes]
	 contents of file2 [in ASCII]
	 */

	FILE * tar_file = fopen(argv[1], "w+");

	if(argv[2] == NULL)
		exit(1);
	
	/* Get file name */
	char* filename  = argv[2];
	int j = 0;
	char str_filename[100];

	/* Add padding to the file name */
	for(j = 0; j < 100; j++) {
		if(j <= strlen(filename))
		{
			str_filename[j] = filename[j];
		}
		else
		{
			str_filename[j] = 0;
		}
	}

	fwrite(str_filename, 100, 1, tar_file);

	// Get file information
	struct stat buf;
	
	int err = stat(filename, &buf);
	if(err != 0) 
	{
		printf("wis-tar: cannot open file\n");
		exit(1);
	} 
	// Write the file size to the tar file
	fwrite(&buf.st_size, 8, 1, tar_file);
	
	// Retrieve the file contents
	FILE * f = fopen(filename, "r");
	if (f == NULL) 
	{
		printf("wis-tar: cannot open file\n");
		exit(1);
	}

        char * line = NULL;
        size_t len = 0;
        ssize_t read;
	size_t file_index = 3;

        /*
        * Read the lines in the file
        */
        while((read = getline(&line, &len, f)) != -1) {
               	fprintf(tar_file, "%s", line);
	}

	/*************************************************
         * Iterate the file inputs
         */
        while ( file_index < argc ) {
		/* Get file name */
	        filename  = argv[3];
        	// int j = 0;

        	/* Add padding to the file name */
        	for(j = 0; j < 100; j++) {
                	if(j <= strlen(filename))
                	{
                        	str_filename[j] = filename[j];
                	}
                	else
                	{
                        	str_filename[j] = 0;
                	}
        	}
		
		fwrite(str_filename, 100, 1, tar_file);

	        // file size
        	int err = stat(filename, &buf);
        	// Do error checking here!
        	if(err != 0)
        	{
			 printf("****2\n");
                	exit(1);
        	}
        	fwrite(&buf.st_size, 8, 1, tar_file);

		// file contents
	        f = fopen(filename, "r");
        	if (f == NULL)
        	{
                	printf("wis-tar: cannot open file\n");
        		exit(1);
		}

	        /*
        	* Read the lines in the file
        	*/
        	while((read = getline(&line, &len, f)) != -1) {
                	fprintf(tar_file, "%s", line);
        	}
		file_index++;
	}

	return 0;
}
