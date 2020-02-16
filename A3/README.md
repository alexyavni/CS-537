Alexandra Yavnilovitch
Assignment 2A

I implement the logic of the shell using the following hierarchy:

	1. Check Whether the input includes multiple expressions (separated by a semicolon). If so - execute sequentially.
	2. Check if the input includes '&' symbols for parallel commands. If so - fork and execute concurrently 
	3. PARSE LINE:
		- Check whether the command is a redirection. If so, redirect stdout and stderr.
		- Check if it is a built in command (exit, cd, path).
		- Default to execute a regular shell command.

There are several helper methods, including:
- numArgs - returns the number of arguments and creates arg list
- Linked List helper methods (add, remove, clear)
- checkAccess: Check access for a command
- pathAction: perform the add / remove / clear actions with the linked list
- checkExit: Checks whether exit is entered and whether the format is correct
- checkRedirect: check whether there are redirect '>' symbols
