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
pthread_t * reduce_threads;
char ** filenames;

void printKeys(int index);
void printKeys_p(int index);
int addPair(char *key, char* value, int index);
int addPair_Partition(char *key, char* value, unsigned long partition);

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
    Combiner combiner;
    CombineGetter getter;
} MapperArgs;

typedef struct ReducerArgs
{
    Reducer reducer;
    ReduceGetter getter;
    int partitionNum;
} ReducerArgs;

struct Key ** map_structs;
struct Key ** reduce_structs;
struct MapperArgs ** mapArgs;
struct ReducerArgs ** reduceArgs;

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
    //printf("In combine GETTER\n");
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

    struct Key * head = map_structs[index];
    struct Key * prev = head;
    while(head != 0)
    {
        if(strcmp(head->key, key) == 0)
        {
            prev->next = head->next;
            char * ret = malloc(100);
            strcpy(ret, head->value);
            free(head->key);
            free(head->value);
            free(head);
            return ret;
        }
        prev = head;
        head = head->next;
    }
    return NULL;
}

char *MyReducerGetter(char *key, int partition_number)
{
    
    struct Key * head = reduce_structs[partition_number];
    struct Key * prev = head;
    while(head != 0)
    {
        if(strcmp(head->key, key) == 0)
        {
            prev->next = head->next;
            char * ret = malloc(100);
            strcpy(ret, head->value);
            free(head->key);
            free(head->value);
            free(head);
            return ret;
        }
        prev = head;
        head = head->next;
    }
    return NULL;
}

void MR_EmitToCombiner(char *key, char *value)
{
    printf("EMIT key = %s\n", key);
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

void MR_EmitToReducer(char *key, char *value)
{
    //printf("###     Inside emit to reducer KEY = %s, VAL = %s\n", key, value);
    unsigned long partition = MR_DefaultHashPartition(key, NUM_reducers);
    addPair_Partition(key, value, partition);
    //printf("EMIT PARTITION %ld \n       key = %s, value = %s\n", partition, key, value);
    //printKeys_p(partition);
}

/*
* Add a new pair to the combiner data structure
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

/*
* Add a new pair to the combiner data structure
*/
int addPair_Partition(char *key, char* value, unsigned long partition)
{
        struct Key * head = reduce_structs[partition];
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

    printf("\n****************************\n");
    printf("INDEX %d\n", index);
    struct Key * head = map_structs[index];
    while(head != 0)
    {
        printf("%s -> ", head->key);
        head = head->next;
    }

    printf("\n****************************\n");
}

void printKeys_p(int index)
{

    printf("\n****************************\n");
    printf("PARTITION %d\n", index);
    struct Key * head = reduce_structs[index];
    while(head != 0)
    {
        printf("%s -> ", head->key);
        head = head->next;
    }

    printf("\n****************************\n");
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
    map(mapper_args->filename); // Is there a return ?
    // How do we use whatever came out of the mapper?


    pthread_t curr_thread = pthread_self();
    int index = -1;
    for(int i = 0; i < NUM_mappers; i++)
    {
        if(map_threads[i] == curr_thread)
        {
            printf("found thread!!");
            index = i;
            break;
        }
    }

    printKeys(index);

    // After mapper is done, invoke combiner, etc.
    Combiner combine = mapper_args->combiner;
    Key * head = map_structs[index];
    while(head->next != 0)
    {
        //printf("IN COMB: index %d, key: %s\n", index, head->next->key);
        char* key = malloc(strlen(head->next->key));
        strcpy(key, head->next->key);
        combine(key, (mapper_args->getter));
        free(key);
    }

    return 0;
}

void *reducer_wrapper(ReducerArgs *reducer_args)
{
    printf("IN REDUCER WRAPPER******************************\n");
    int partition_num = reducer_args->partitionNum;
    Reducer reduce = reducer_args->reducer;
    ReduceGetter getter = reducer_args->getter;
    
    Key * head = reduce_structs[partition_num];
    while(head->next != 0)
    {
        //printf("IN COMB: index %d, key: %s\n", index, head->next->key);
        char* key = malloc(strlen(head->next->key));
        strcpy(key, head->next->key);
        reduce(key, NULL, getter, partition_num);
        free(key);
    }

    return 0;
}

void MR_Run(int argc, char *argv[], Mapper map, int num_mappers, Reducer reduce, int num_reducers,
            Combiner combine, Partitioner partition)
{
    printf("Inside of MR_Run \n     argc = %d\n     num mappers = %d\n", argc, num_mappers);
    // Step 1: Create data structures that will hold intermediate data
    NUM_mappers = num_mappers;
    NUM_reducers = num_reducers;
    // create (num_mappers) data structure for each mapper
    reduce_structs = malloc(num_reducers * sizeof(struct Key));
    map_structs = malloc(num_mappers * sizeof(struct Key));
    filenames = malloc((argc-1) * sizeof(char*));
    mapArgs = malloc(num_mappers * sizeof(MapperArgs*));
    reduceArgs = malloc(num_reducers * sizeof(ReducerArgs*));

    CombineGetter combine_getter;
    combine_getter = MyCombinerGetter;

    ReduceGetter reduce_getter;
    reduce_getter = MyReducerGetter;

    for(int j = 0; j < (argc-1); j++)
    {
        printf("argv[%d] = %s\n", j, argv[j+1]);
        filenames[j] = malloc(sizeof(argv[j+1]));
        mapArgs[j] = malloc(sizeof(MapperArgs));
        mapArgs[j]->filename = argv[j+1];
        mapArgs[j]->map = map;
        mapArgs[j]->combiner = combine;
        mapArgs[j]->getter = combine_getter;
    }

    for(int k = 0; k < num_reducers; k++)
    {
        reduce_structs[k] = (struct Key *)malloc(sizeof(struct Key));
        reduce_structs[k]->key = (char *)malloc(sizeof("HEAD"));
        reduce_structs[k]->key = "HEAD";
        reduce_structs[k]->value = 0;
        reduce_structs[k]->next = 0;
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
    void* (*r_wrapper)();
    r_wrapper = reducer_wrapper;
    // map_threads array of threads from 0 to num_mappers
    map_threads = malloc(num_mappers * sizeof(pthread_t));
    for (int i = 0; i < num_mappers; ++i)
    {
        printf("test in thread loop: file = %s\n", mapArgs[i]->filename);
        pthread_create(&map_threads[i], NULL, m_wrapper, mapArgs[i]);
        if(i >= argc - 2) break;
    }
    // Wait for threads to exit
    for (int i = 0; i < num_mappers; i++)
    {
       pthread_join(map_threads[i], NULL);
    }

    // Step 3: Launch reduce threads (simple mode) to process intermediate data
    reduce_threads = malloc(num_reducers * sizeof(pthread_t));
    for (int i = 0; i < num_reducers; ++i)
    {
        printf("generating thread i for reducer\n");
        reduceArgs[i] = malloc(sizeof(ReducerArgs));
        reduceArgs[i]->getter = reduce_getter;
        reduceArgs[i]->partitionNum = i;
        reduceArgs[i]->reducer = reduce;
        //printf("test in thread loop (REDUCE)\n");
        pthread_create(&reduce_threads[i], NULL, r_wrapper, reduceArgs[i]);
        //if(i >= num_reducers) break;
    }

    // Wait for threads to exit
    for (int i = 0; i < num_reducers; i++)
    {
       pthread_join(reduce_threads[i], NULL);
    }

    printf("\n\nALL DONE!!!!!!!!!!!!!!!!!!\n");

}