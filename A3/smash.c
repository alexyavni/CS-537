#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/* Linked List structure */
typedef struct Node {
    char *data;
    struct Node* next;
} Node;

int numArgs(char *line, char** args_list);
int checkExit(char *func, int num_args); 
int path_action(char **args_list, int num_args, struct Node **path_list);
int add_node(char*path, struct Node **path_list);
int remove_node(char *path, struct Node **path_list);
int clear_path_list(struct Node **path_list);
int print_LL(struct Node **path_list);
void err();

static char *EXIT = "exit";
static char *SPACES = "  \t\r\n\v\f";
static char ERR_MSG[30] = "An error has occurred\n";

int main(int argc, char *argv[]) {
   /* Print first prompt */
   printf("smash> ");

   /* Variables for reading lines */
   char *buffer = NULL;
   size_t n = 0;

   /* Path list and the default path */
   struct Node* head = (struct Node*)malloc(sizeof(struct Node));
   struct Node* default_path = (struct Node*)malloc(sizeof(struct Node));

   head -> data = "HEAD";
   head -> next = default_path;

   char * def_path = (char*) malloc(strlen("/bin"));
   strcpy(def_path, "/bin");
   default_path -> data = def_path;
   default_path -> next = NULL;
   struct Node **list = &head;

   while(1) 
   {
	if(getline(&buffer, &n, stdin) != -1)
	{
		/* Parse the user input  */
		char *input = strtok(buffer, SPACES);

		/* First arg is the function name */
		char *functionName = input;
		char** args_list = malloc(1000);

		int num_args = numArgs(input, args_list);

		// printf("Num ARGS = %d\n", num_args);
		// printf("Function = %s\n", functionName);
		
		/* Does the user want to exit? */
		if(checkExit(functionName, num_args))
			exit(0);
		
		if(strcmp(functionName, "path") == 0)
		{
			path_action(args_list, num_args, list);
		}

		if(strcmp(functionName, "cd") == 0)
		{
			char s[100]; 
			printf("Curr dir: %s\n", getcwd(s, 100));
			if(num_args != 2) err();
			else 
			{ 
				int cd_error = chdir(args_list[1]);
				if(cd_error != 0) err();
				printf("Curr dir: %s\n", getcwd(s, 100));
			}
		}
		printf("smash> ");
	}
   }

   return 0;
}

/* Helper function to return the 
 * number of arguments in the user input 
 * 	param: 	one line of input
 * 	return:	number of arguments (including function name)
 */
int numArgs(char *line, char** args_list)
{
	int args = 0;
	int pos = 0;
	while(line != NULL)
        {
		if(strcmp(line, " ") != 0)
		{
			args_list[args] = malloc(strlen(line));
			args_list[args] = line;
			args ++;
		}
		line = strtok(NULL, SPACES);
        }
	return args;
}

/*
 * Helper function to check if user wants to exit smash
 */
int checkExit(char *func, int num_args)
{
	if(strcmp(func, EXIT) == 0)
	{
		if(num_args == 1)
		{
			return 1;
		}
		else
		{
			err();
		}
	}
	return 0;

}

/*
* Execute path action (add, remove, clear)
*/
int path_action(char **args_list, int num_args, struct Node **path_list)
{
	// printf("*********************** inside path_action\n");
	// printf("NUM ARGS = %d\n", num_args);
	if(num_args < 2) 
	{
		err();
		return 1;
	}
	// case clear
	if(num_args == 2 && strcmp(args_list[1], "clear") == 0)
	{
		clear_path_list(path_list);
		// printf("*CLEAR*\n");
		print_LL(path_list);
		// printf("\n");
		return 0;
	}

	char* curr_path;
	if(num_args == 3)
	{
		curr_path = (char*) malloc(strlen(args_list[2]));
		strcpy(curr_path, args_list[2]);
	}
	else err();

	// case add
	if(strcmp(args_list[1], "add") == 0 && num_args == 3)
	{
		// printf("*ADD*\n");
		add_node(curr_path, path_list);
	}

	// case remove
	if(strcmp(args_list[1], "remove") == 0 && num_args == 3)
	{
		// printf("*REMOVE*\n");
		remove_node(curr_path, path_list);
	}

	print_LL(path_list);
	// printf("\n");
	return 0;	
}

/*
* Add a new path to the path list
*/
int add_node(char *path, struct Node **path_list) 
{
	struct Node* new_node = (struct Node*) malloc(sizeof(struct Node));
	new_node -> data = path;

	struct Node * curr = *path_list;
	struct Node * temp = curr->next;
	curr -> next = new_node;
	new_node -> next = temp;
	
	return 0;

}

/*
* Remove a path from the list (and all duplicates)
*/
int remove_node(char *path, struct Node **path_list) 
{
	struct Node *curr = *path_list;
	struct Node *prev;
	int found = 0;
	while( curr->next != NULL)
	{	
		prev = curr;
		curr = curr -> next;
		// printf("str diff: %d\n", strcmp(curr->data, path));
		if(strcmp(curr->data, path) == 0)
		{
			prev -> next = curr -> next;
			free(curr->data);
			curr = prev;
			found = 1;
		}
	}
	if(!found)
		err();
	return 1;
}

/*
* Clear the path list - should be empty
*/
int clear_path_list(struct Node **path_list)
{
	struct Node *curr = *path_list;
	struct Node *head = *path_list;
	while( curr->next != NULL)
	{
		curr = curr -> next;
		free(curr -> data);
	}
	head -> next = NULL;
}

/*
* Helper function
* Print the linked list
*/
int print_LL(struct Node **path_list)
{
	
	struct Node *curr = *path_list;
	printf("%s -> ", curr->data);
	while(curr->next != NULL)
	{
		curr = curr->next;
		printf("%s -> ", curr->data);	
	}
	printf("\n");

	return 0;
}

/*
* Helper function
* Prints the same error message
*/
void err()
{
	write(STDERR_FILENO, ERR_MSG, strlen(ERR_MSG));
}
