# A1
## Alexandra Yavnilovitch

### wis-grep.c
```bash
For the wis-grep implementation, I first verified that the number of command line arguments is correct, and that the specified filenames exist.
Nest, I created an outer loop to iterate through all the specified files. Within each file instance, I go line by line, and check for the search string.
Whenever an instance of a string is found, that line is printed to the console.
```

### wis-tar.c
```bash
For the wis-tar implementation, I first verified the command line arguments and input files. Then, I used 100 bytes to represent the file name (either the first 100 bytes, or the full name with null characters). Next, I extracted the file size using the stat function, and represented it using 8 bytes. Finally, I represented the ASCII values of the contents of the file. These 3 elements (of each specified file), are stored in the new tar file.
```

### wis-tar.c
```bash
For the wis-untar implementation, I first verified the command line arguments and file input. Next, I created the external loop to read information about each file. Within the loop, The first stage is to retrieve the first 100 bytes, which represent the file name. Next, I retrieve the following 8 bytes, which represent the given file size. Finally, using the file size, I extracted the contents of the file. With all of the retrieved elements, I recreated the original files.
```

Resources used: Stack Overflow, TA office hours.


