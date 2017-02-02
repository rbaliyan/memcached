#ifndef _HASH_TABLE_H_
#define _HASH_TABLE_H_

#include "avl.h"
#include "cache_data.h"
#include "pthread.h"

typedef avl_node_t hash_data_node_t;
typedef struct hash_node_s
{
    int index;
    avl_tree_t *tree;
    int reader_count;
    int writer_here;
    pthread_mutex_t lock;
    pthread_cond_t reader_can_enter;
    pthread_cond_t writer_can_enter;
} hash_node_t;

typedef struct hashtable_s
{
    uint32_t size;
    hash_node_t table[0];
}hash_table_t;

void hash_table_init(void);
hash_table_t* hash_table_create( uint32_t size);
int hash_table_insert(hash_table_t *ht, cache_data_t* data,hash_data_node_t **dup);
hash_data_node_t* hash_table_search(hash_table_t *ht, cache_data_t* data);
void hash_table_destroy(hash_table_t *ht);
cache_data_t* hash_get_cache(hash_data_node_t *hnode);


#endif
