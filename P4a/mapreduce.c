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
    //    int size;
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
struct Key **map_removed = NULL;
struct Key **reduce_structs = NULL;
struct Key **reduce_removed = NULL;

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
            char *ret = malloc(strlen(head->value) + 1);
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

    struct Key *prev = reduce_structs[partition_number];
    struct Key *head = prev->next;
    while (head != 0)
    {
        if (strcmp(head->key, key) == 0)
        {
            prev->next = head->next;
            char *ret = malloc(strlen(head->value) + 1);
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
    int index = getMyMapperThreadIndex();
    addPair(key, value, index);
}

void MR_EmitToReducer(char *key, char *value)
{
    unsigned long partition;
    if(partition_function == NULL)
        partition = MR_DefaultHashPartition(key, NUM_reducers);
    else partition = partition_function(key, NUM_reducers);
    
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
    if (strcmp(key, "ipsum") == 0)
    {
        count_ipsum++;
    }
    newPair->next = head->next;
    head->next = newPair;
    pthread_mutex_unlock(&list_mtx);

    return 0;
}

void *mapper_wrapper(MapperArgs *mapper_args)
{
    int thread_index = getMyMapperThreadIndex();

    int no_combiner = 0;
    if (mapper_args->combiner == NULL)
        no_combiner = 1;
    // This function is called by map threads
    // We will invoke mapper here
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
    Combiner combine = mapper_args->combiner;
    Key *head = map_structs[thread_index];
    mergeSort(&(head->next));

    if (no_combiner == 0)
    {
        while (head->next != 0)
        {
            char *key = malloc(strlen(head->next->key));
            strcpy(key, head->next->key);
            combine(key, (mapper_args->getter));
            free(key);
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
        char *key = malloc(strlen(head->next->key));
        strcpy(key, head->next->key);
        reduce(key, NULL, getter, partition_num);
    }
    return 0;
}

void MR_Run(int argc, char *argv[], Mapper map, int num_mappers, Reducer reduce, int num_reducers,
            Combiner combine, Partitioner partition)
{
    partition_function = partition;
    NUM_mappers = num_mappers;
    NUM_reducers = num_reducers;
    reduce_structs = malloc(num_reducers * sizeof(struct Key));
    map_structs = malloc(num_mappers * sizeof(struct Key));
    filenames = malloc((argc - 1) * sizeof(char *));
    mapArgs = malloc(num_mappers * sizeof(MapperArgs));
    reduceArgs = malloc(num_reducers * sizeof(ReducerArgs));

    CombineGetter combine_getter;
    combine_getter = MyCombinerGetter;

    ReduceGetter reduce_getter;
    reduce_getter = MyReducerGetter;
    num_files = argc - 1;

    for (int j = 0; j < (argc - 1); j++)
    {
        filenames[j] = malloc(strlen(argv[j + 1]) + 1);
        strcpy(filenames[j], argv[j + 1]);
        mapArgs[j] = malloc(sizeof(MapperArgs));
        mapArgs[j]->filename = malloc(strlen(filenames[j]) + 1);
        strcpy(mapArgs[j]->filename, argv[j + 1]);
        mapArgs[j]->map = map;
        mapArgs[j]->combiner = combine;
        mapArgs[j]->getter = combine_getter;
    }

    for (int k = 0; k < num_reducers; k++)
    {
        reduce_structs[k] = (struct Key *)malloc(sizeof(struct Key));
        reduce_structs[k]->key = (char *)malloc(strlen("HEAD") + 1);
        strcpy(reduce_structs[k]->key, "HEAD");
        reduce_structs[k]->value = 0;
        reduce_structs[k]->next = 0;
    }

    for (int i = 0; i < num_mappers; i++)
    {
        map_structs[i] = (struct Key *)malloc(sizeof(struct Key));
        map_structs[i]->key = (char *)malloc(strlen("HEAD") + 1);
        strcpy(map_structs[i]->key, "HEAD");
        map_structs[i]->value = 0;
        map_structs[i]->next = 0;
    }

    // Step 2: Launch threads to run map function
    //         Launch num_mappers threads: pthread accepts one argument
    void *(*m_wrapper)();
    m_wrapper = mapper_wrapper;
    void *(*r_wrapper)();
    r_wrapper = reducer_wrapper;
    map_threads = malloc(num_mappers * sizeof(pthread_t));
    file_index = num_mappers;
    for (int i = 0; i < num_mappers; i++)
    {
        pthread_create(&map_threads[i], NULL, m_wrapper, mapArgs[i]);
    }
    // Wait for threads to exit
    for (int i = 0; i < num_mappers; i++)
    {
        pthread_join(map_threads[i], NULL);
    }

    // Step 3: Launch reduce threads (simple mode) to process intermediate data
    reduce_threads = malloc(num_reducers * sizeof(pthread_t));
    for (int i = 0; i < num_reducers; i++)
    {
        reduceArgs[i] = malloc(sizeof(ReducerArgs));
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

/* Merge function of Merge Sort to Merge the two sorted parts 
   of the Linked List. We compare the next value of start1 and  
   current value of start2 and insert start2 after start1 if  
   it's smaller than next value of start1. We do this until 
   start1 or start2 end. If start1 ends, then we assign next  
   of start1 to start2 because start2 may have some elements 
   left out which are greater than the last value of start1.  
   If start2 ends then we assign end2 to end1. This is necessary 
   because we use end2 in another function (mergeSort function)  
   to determine the next start1 (i.e) start1 for next 
   iteration = end2->next */
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
    // printf("CHECKING LEN length = %d\n", (*head)->size);
    int len = length(*head);
    // printf("Done checking len - new = %d\n", len);

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

