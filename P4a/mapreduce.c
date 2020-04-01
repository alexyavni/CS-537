#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <string.h>
// #include <sys.h>
#include "mapreduce.h"
#include <semaphore.h>

int NUM_reducers;
int NUM_mappers;
pthread_t * map_threads;
char ** filenames;
//typedef void (*Mapper)(char *file_name);
//typedef void (*Combiner)(char *key, CombineGetter get_next);

void printKeys(int index);
int addPair(char *key, char* value, int index);

typedef struct Key
{
    char *key;
    char *value;
    struct Key *next;
} Key;

typedef struct MapperArgs
{
    char *filename;
    Mapper map;
} MapperArgs;

struct Key ** map_structs;
struct MapperArgs ** mapArgs;

unsigned long
MR_DefaultHashPartition(char *key, int num_partitions)
{
    unsigned long hash = 5381;
    int c;
    while ((c = *key++) != '\0')
        hash = hash * 33 + c;
    return hash % num_partitions;
}

char *MyCombinerGetter(char *key)
{
    return "blah";
}

char *MyReducerGetter(char *key, int partition_number)
{
    return "blah";
}

void MR_EmitToCombiner(char *key, char *value)
{
    pthread_t curr_thread = pthread_self();
    int index = -1;
    for(int i = 0; i < NUM_mappers; i++)
    {
        if(map_threads[i] == curr_thread)
        {
            index = i;
            break;
        }
    }

    if(*key != 0)
    {
        addPair(key, value, index);
        //printf("KEY = %s\n", key);
    }
}

/*
* Add a new path to the path list
*/
int addPair(char *key, char* value, int index)
{
        struct Key * head = map_structs[index];
        struct Key * newPair = (struct Key *)malloc(sizeof(struct Key *));

        char* key_copy = malloc(strlen(key));
        char* val_copy = malloc(strlen(value));
        strcpy(key_copy,key);
        strcpy(val_copy,value);

        newPair->key = key_copy;
        newPair->value = val_copy;
        newPair->next = head->next;
        head->next = newPair;

        return 0;
}

void printKeys(int index)
{
    printf("INDEX = %d\n", index);
    printf("\n");
    struct Key * head = map_structs[index];
    while(head != 0)
    {
        printf("%s -> ", head->key);
        head = head->next;
    }

    printf("\n");
}

void MR_EmitToReducer(char *key, char *value)
{
}

// Organize in (key,val) pairs in a data structure
// [head] -> [key1] -> [key2] -> [key3] -> NULL
//             |         |          |
//            [1]       [1]        [1]
//             |         |          |
//            [1]       null       [1]
//             |                    |
//            null                 null
void *mapper_wrapper(MapperArgs *mapper_args)
{
    // This function is called by map threads
    // We will invoke mapper here
    printf("in mapper wrapper\n");
    Mapper map = mapper_args->map;
    int pt = pthread_self();
    printf("curr thread = %d, filename = %s\n", pt, mapper_args -> filename);
    map(mapper_args->filename); // Is there a return ?
    // How do we use whatever came out of the mapper?

    // After mapper is done, invoke combiner, etc.
    return 0;
}

void MR_Run(int argc, char *argv[], Mapper map, int num_mappers, Reducer reduce, int num_reducers,
            Combiner combine, Partitioner partition)
{
    printf("Inside of MR_Run \n     argc = %d\n    num mappers = %d\n", argc, num_mappers);
    // Step 1: Create data structures that will hold intermediate data
    NUM_mappers = num_mappers;
    NUM_reducers = num_reducers;
    // create (num_mappers) data structure for each mapper
    map_structs = malloc(num_mappers * sizeof(struct Key));
    filenames = malloc((argc-1) * sizeof(char*));
    mapArgs = malloc(num_mappers*sizeof(MapperArgs*));
    for(int j = 0; j < (argc-1); j++)
    {
        printf("argv[%d] = %s\n", j, argv[j+1]);
        filenames[j] = malloc(sizeof(argv[j+1]));
        mapArgs[j] = malloc(sizeof(MapperArgs));
        mapArgs[j]->filename = argv[j+1];
        mapArgs[j]->map = map;
    }

    for (int i = 0; i < num_mappers; i++)
    {
        map_structs[i] = (struct Key *)malloc(sizeof(struct Key));
        map_structs[i]->key = (char *)malloc(sizeof("HEAD"));
        map_structs[i]->key = "HEAD";
        map_structs[i]->value = 0;
        map_structs[i]->next = 0;
    }

    //struct MapperArgs *mapper_args = (struct MapperArgs *)malloc(sizeof(struct MapperArgs));
    //mapper_args->map = map;
    //mapper_args->filename = malloc(sizeof(char *));

    // Step 2: Launch threads to run map function
    //         Launch num_mappers threads: pthread accepts one argument
    void* (*m_wrapper)();
    m_wrapper = mapper_wrapper;
    // map_threads array of threads from 0 to num_mappers
    map_threads = malloc(num_mappers * sizeof(pthread_t));
    for (int i = 0; i < num_mappers; ++i)
    {
        printf("test in thread loop: file = %s\n", mapArgs[i]->filename);
        pthread_create(&map_threads[i], NULL, m_wrapper, mapArgs[i]);
        //printf("GOT HERE\n");
        if(i >= argc - 2) break;
    }
    // Wait for threads to exit
    for (int i = 0; i < num_mappers; i++)
    {
       //printf("GOT HERE 2\n");
       pthread_join(map_threads[i], NULL);
    }

    for (int i = 0; i < argc-1; i++)
    {
       printKeys(i);
    }
    


    // Step 3: Launch reduce threads (simple mode) to process intermediate data
}