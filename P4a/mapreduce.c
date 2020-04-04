#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <string.h>
#include "mapreduce.h"
#include <semaphore.h>

int NUM_reducers = 0;
int NUM_mappers = 0;
pthread_t *map_threads = NULL;
pthread_t *reduce_threads = NULL;
char **filenames = NULL;
int num_files = 0;
volatile int file_index = 0;
pthread_mutex_t file_mtx = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t list_mtx = PTHREAD_MUTEX_INITIALIZER;
// pthread_mutex_t dictum_mtx = PTHREAD_MUTEX_INITIALIZER;

int count_ipsum = 0;
int count_dictum = 0;

void printKeys(int index);
void printKeys_p(int index);
int addPair(char *key, char *value, int index);
int addPair_Partition(char *key, char *value, unsigned long partition);
Partitioner partition_function;

typedef struct Key
{
    char *key;
    char *value;
    struct Key *next;
} Key;

void mergeSort(struct Key **head);
void merge(struct Key **start1, struct Key **end1,
           struct Key **start2, struct Key **end2);
int length(struct Key *current);

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

struct Key **map_structs = NULL;
struct Key **reduce_structs = NULL;

struct Key *map_removed = NULL;
struct Key *reduce_removed = NULL;

struct MapperArgs **mapArgs = NULL;
struct ReducerArgs **reduceArgs = NULL;

unsigned long
MR_DefaultHashPartition(char *key, int num_partitions)
{
    unsigned long hash = 5381;
    int c;
    while ((c = *key++) != '\0')
        hash = hash * 33 + c;
    return hash % num_partitions;
}

int getMyMapperThreadIndex()
{
    pthread_t curr_thread = pthread_self();
    int index = -1;
    for (int i = 0; i < NUM_mappers; i++)
    {
        if (map_threads[i] == curr_thread)
        {
            index = i;
            break;
        }
    }
    return index;
}

char *MyCombinerGetter(char *key)
{
    int index = getMyMapperThreadIndex();

    struct Key *prev = map_structs[index];
    struct Key *head = prev->next;
    while (head != 0)
    {
        if (strcmp(head->key, key) == 0)
        {
            prev->next = head->next;
            head->next = map_removed->next;
            map_removed->next = head;
            return (head->value);
        }
        prev = head;
        head = head->next;
    }
    return NULL;
}

char *MyReducerGetter(char *key, int partition_number)
{

    struct Key *prev = reduce_structs[partition_number];
    struct Key *head = prev->next;
    while (head != 0)
    {
        if (strcmp(head->key, key) == 0)
        {
            prev->next = head->next;
            head->next = reduce_removed->next;
            reduce_removed->next = head;
            return (head->value);
        }
        prev = head;
        head = head->next;
    }
    return NULL;
}

void MR_EmitToCombiner(char *key, char *value)
{
    int index = getMyMapperThreadIndex();
    addPair(key, value, index);
}

void MR_EmitToReducer(char *key, char *value)
{
    unsigned long partition;
    if (partition_function == NULL)
        partition = MR_DefaultHashPartition(key, NUM_reducers); // Default partitioner
    else
        partition = partition_function(key, NUM_reducers); // Custom partitioner

    addPair_Partition(key, value, partition);
}

/*
* Add a new pair to the combiner data structure
*/
int addPair(char *key, char *value, int index)
{
    struct Key *head = map_structs[index];
    struct Key *newPair = (struct Key *)malloc(sizeof(struct Key));
    newPair->key = malloc(strlen(key) + 1);
    newPair->value = malloc(strlen(value) + 1);

    strcpy(newPair->key, key);
    strcpy(newPair->value, value);

    newPair->next = head->next;
    head->next = newPair;

    return 0;
}

/*
* Add a new pair to the combiner data structure
*/
int addPair_Partition(char *key, char *value, unsigned long partition)
{
    struct Key *head = reduce_structs[partition];
    struct Key *newPair = (struct Key *)malloc(sizeof(struct Key));
    newPair->key = malloc(strlen(key) + 1);
    newPair->value = malloc(strlen(value) + 1);

    strcpy(newPair->key, key);
    strcpy(newPair->value, value);

    pthread_mutex_lock(&list_mtx);
    newPair->next = head->next;
    head->next = newPair;
    pthread_mutex_unlock(&list_mtx);

    return 0;
}

void *mapper_wrapper(MapperArgs *mapper_args)
{
    int thread_index = getMyMapperThreadIndex();
    Combiner combine = mapper_args->combiner;
    Key *head = map_structs[thread_index];

    int no_combiner = 0;
    if (mapper_args->combiner == NULL)
    {
        no_combiner = 1;
    }

    Mapper map = mapper_args->map;
    map(mapper_args->filename);

    int files_left = 1;
    while (files_left == 1)
    {
        pthread_mutex_lock(&file_mtx);
        int f_index = file_index;
        file_index++;
        pthread_mutex_unlock(&file_mtx);

        if (f_index < num_files)
        {
            map(filenames[f_index]);
        }
        else
            files_left = 0;
    }

    // After mapper is done, invoke combiner (if used)
    mergeSort(&(head->next));

    if (no_combiner == 0)
    {
        while (head->next != 0)
        {
            combine(head->next->key, (mapper_args->getter));
        }
    }

    return 0;
}

void *reducer_wrapper(ReducerArgs *reducer_args)
{
    int partition_num = reducer_args->partitionNum;
    Reducer reduce = reducer_args->reducer;
    ReduceGetter getter = reducer_args->getter;

    Key *head = reduce_structs[partition_num];

    mergeSort(&(head->next));
    while (head->next != 0)
    {
        reduce(head->next->key, NULL, getter, partition_num);
    }
    return 0;
}

void MR_Run(int argc, char *argv[], Mapper map, int num_mappers, Reducer reduce, int num_reducers,
            Combiner combine, Partitioner partition)
{
    // Initialize parameter variables
    partition_function = partition;
    NUM_mappers = num_mappers;
    NUM_reducers = num_reducers;
    num_files = argc - 1;

    // Combiner
    CombineGetter combine_getter;
    combine_getter = MyCombinerGetter;

    // Reducer
    ReduceGetter reduce_getter;
    reduce_getter = MyReducerGetter;

    // Declare and allocate global structs
    reduce_structs = malloc(num_reducers * sizeof(struct Key)); // Reduce structure
    map_structs = malloc(num_mappers * sizeof(struct Key));     // Mapper structure
    filenames = malloc(num_files * sizeof(char *));             // List of file names
    mapArgs = (struct MapperArgs **)malloc(num_mappers * sizeof(struct MapperArgs));         // List of mapper arguments
    reduceArgs = malloc(num_reducers * sizeof(struct ReducerArgs));    // List of reducer arguments
    map_threads = malloc(num_mappers * sizeof(pthread_t));      // Allocate mapper threads
    reduce_threads = malloc(num_reducers * sizeof(pthread_t));  // Allocate reducer threads
    file_index = num_mappers;                                   // File index - begins as the number of files initially sent to mapper
                                                                // then incremented until all files are processed

    // Function pointers
    void *(*m_wrapper)();
    m_wrapper = mapper_wrapper; // Assign mapping & combining wrapper function
    void *(*r_wrapper)();
    r_wrapper = reducer_wrapper; // Assign reducer wrapper function

    // Assign filename list and mapper arguments
    for (int j = 0; j < num_files; j++)
    {
        filenames[j] = malloc(strlen(argv[j + 1]) + 1);
        strcpy(filenames[j], argv[j + 1]);
    }
    for (int j = 0; j < num_mappers; j++)
    {
        mapArgs[j] = (struct MapperArgs*)malloc(sizeof(struct MapperArgs));
        mapArgs[j]->filename = malloc(strlen(filenames[j]) + 1);
        strcpy(mapArgs[j]->filename, argv[j + 1]);
        mapArgs[j]->map = map;
        mapArgs[j]->combiner = combine;
        mapArgs[j]->getter = combine_getter;
    }

    // Create the header for MAPPER data structure
    for (int i = 0; i < num_mappers; i++)
    {
        map_structs[i] = (struct Key *)malloc(sizeof(struct Key));
        map_structs[i]->key = (char *)malloc(strlen("HEAD") + 1);
        strcpy(map_structs[i]->key, "HEAD");
        map_structs[i]->value = 0;
        map_structs[i]->next = 0;
    }

    // Create the header for REDUCER data structure
    for (int k = 0; k < num_reducers; k++)
    {
        reduce_structs[k] = (struct Key *)malloc(sizeof(struct Key));
        reduce_structs[k]->key = (char *)malloc(strlen("HEAD") + 1);
        strcpy(reduce_structs[k]->key, "HEAD");
        reduce_structs[k]->value = 0;
        reduce_structs[k]->next = 0;
    }

    map_removed = (struct Key *)malloc(sizeof(struct Key));
    map_removed->key = (char *)malloc(strlen("MAP REMOVED") + 1);
    strcpy(map_removed->key, "MAP REMOVED");
    map_removed->value = 0;
    map_removed->next = 0;

    reduce_removed = (struct Key *)malloc(sizeof(struct Key));
    reduce_removed->key = (char *)malloc(strlen("REDUCE REMOVED") + 1);
    strcpy(reduce_removed->key, "REDUCE REMOVED");
    reduce_removed->value = 0;
    reduce_removed->next = 0;

    // Launch threads to run map function
    for (int i = 0; i < num_mappers; i++)
    {
        pthread_create(&map_threads[i], NULL, m_wrapper, mapArgs[i]);
    }
    // Wait for threads to exit
    for (int i = 0; i < num_mappers; i++)
    {
        pthread_join(map_threads[i], NULL);
    }

    // FREE MAP STRUCTS
    for (int i = 0; i < num_mappers; i++)
    {
        free(map_structs[i]->key);
        free(map_structs[i]);
    }
    free(map_structs);
    struct Key *head_m = map_removed;
    struct Key *tmp_m = head_m;
    while (head_m != 0)
    {
        tmp_m = head_m;
        head_m = head_m->next;
        free(tmp_m->key);
        free(tmp_m->value);
        free(tmp_m);
    }
    for (int j = 0; j < num_files; j++)
    {
        free(filenames[j]);
    }
    free(filenames);
    for (int j = 0; j < num_mappers; j++)
    {
        free(mapArgs[j]->filename);
        free(mapArgs[j]);
    }
    free(mapArgs);
    free(map_threads);

    // Launch reduce threads (simple mode) to process intermediate data
    for (int i = 0; i < num_reducers; i++)
    {
        reduceArgs[i] = malloc(sizeof(struct ReducerArgs));
        reduceArgs[i]->getter = reduce_getter;
        reduceArgs[i]->partitionNum = i;
        reduceArgs[i]->reducer = reduce;
        pthread_create(&reduce_threads[i], NULL, r_wrapper, reduceArgs[i]);
    }
    // Wait for threads to exit
    for (int i = 0; i < num_reducers; i++)
    {
        pthread_join(reduce_threads[i], NULL);
    }

    for (int i = 0; i < num_reducers; i++)
    {
        free(reduce_structs[i]->key);
        free(reduce_structs[i]);
        free(reduceArgs[i]);
    }
    free(reduceArgs);
    free(reduce_structs);
    free(reduce_threads);
    struct Key *head_r = reduce_removed;
    struct Key *tmp_r = head_r;
    while (head_r != 0)
    {
        tmp_r = head_r;
        head_r = head_r->next;
        free(tmp_r->key);
        free(tmp_r->value);
        free(tmp_r);
    }
}

/* 
Iterative Merge Sort related functions: merge, mergeSort, length
Source: https://www.geeksforgeeks.org/iterative-merge-sort-for-linked-list/
*/
void merge(struct Key **start1, struct Key **end1,
           struct Key **start2, struct Key **end2)
{

    // Making sure that first node of second
    // list is higher.
    struct Key *temp = NULL;
    if (strcmp((*start1)->key, (*start2)->key) > 0)
    {
        Key *tmp = *start1;
        *start1 = *start2;
        *start2 = tmp;

        tmp = *end1;
        *end1 = *end2;
        *end2 = tmp;
    }

    // Merging remaining nodes
    struct Key *astart = *start1;
    struct Key *aend = *end1;
    struct Key *bstart = *start2;
    struct Key *bendnext = (*end2)->next;
    while (astart != aend && bstart != bendnext)
    {
        if (strcmp(astart->next->key, bstart->key) > 0)
        {
            temp = bstart->next;
            bstart->next = astart->next;
            astart->next = bstart;
            bstart = temp;
        }
        astart = astart->next;
    }
    if (astart == aend)
        astart->next = bstart;
    else
        *end2 = *end1;
}

/* MergeSort of Linked List */
void mergeSort(struct Key **head)
{
    if (*head == NULL)
        return;
    struct Key *start1 = NULL;
    struct Key *end1 = NULL;
    struct Key *start2 = NULL;
    struct Key *end2 = NULL;
    struct Key *prevend = NULL;
    int len = length(*head);

    for (int gap = 1; gap < len; gap = gap * 2)
    {
        start1 = *head;
        while (start1)
        {

            // If this is first iteration
            int isFirstIter = 0;
            if (start1 == *head)
                isFirstIter = 1;

            // First part for merging
            int counter = gap;
            end1 = start1;
            while (--counter && end1->next)
                end1 = end1->next;

            // Second part for merging
            start2 = end1->next;
            if (!start2)
                break;
            counter = gap;
            end2 = start2;
            while (--counter && end2->next)
                end2 = end2->next;

            // To store for next iteration.
            Key *temp = end2->next;

            // Merging two parts.
            merge(&start1, &end1, &start2, &end2);

            // Update head for first iteration, else
            // append after previous list
            if (isFirstIter == 1)
                *head = start1;
            else
                prevend->next = start1;

            prevend = end2;
            start1 = temp;
        }
        prevend->next = start1;
    }
}

/* Function to calculate length of linked list */
int length(struct Key *current)
{
    int count = 0;
    while (current != NULL)
    {
        current = current->next;
        count++;
    }
    return count;
}

/* Debugging print function for mapper data structure */
void printKeys(int index)
{

    printf("\n****************************\n");
    printf("INDEX %d\n", index);
    struct Key *head = map_structs[index];
    while (head != 0)
    {
        printf("%s -> ", head->key);
        head = head->next;
    }

    printf("\n****************************\n");
}

/* Debugging print function for mapper data structure */
void printKeys_p(int index)
{

    printf("\n****************************\n");
    printf("PARTITION %d\n", index);
    struct Key *head = reduce_structs[index];
    while (head != 0)
    {
        printf("%s -> ", head->key);
        head = head->next;
    }

    printf("\n****************************\n");
}
