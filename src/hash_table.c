#include <malloc.h>
#include "hash_table.h"

#define MODULE "HashTable"
#include "trace.h"

void hash_table_init(void)
{
    /* Initialize AVL Tree */
    avl_init((avl_compare_t)cache_data_cmpkey, (avl_free_data_t)cache_data_free,
             (avl_dump_data_t)cache_data_dump);
             
    TRACE(DEBUG,"AVL Init Done");
}

cache_data_t* hash_get_cache(hash_data_node_t *hnode)
{
    return (cache_data_t*)hnode->data;
}

static int read_write_lock_init(hash_node_t *hnode)
{
    int ret = 0; 
    hnode->reader_count = 0;
    hnode->writer_here = 0;
    
    if (pthread_mutex_init(&hnode->lock, NULL) != 0)
    {
        TRACE(ERROR,"Failed to init mutex");
        ret = -1;
    }
    else if (pthread_cond_init(&hnode->reader_can_enter, NULL) != 0)
    {
        TRACE(ERROR,"Failed to init condition variable");
        pthread_mutex_destroy(&hnode->lock);
        ret = -1;
    }
    else if (pthread_cond_init(&hnode->writer_can_enter, NULL) != 0)
    {
        TRACE(ERROR,"Failed to init condition variable");
        pthread_mutex_destroy(&hnode->lock);
        pthread_cond_destroy(&hnode->reader_can_enter);
        ret = -1;
    }
    
    return ret;
}

static void read_lock(hash_node_t *hnode)
{
    pthread_mutex_lock(&hnode->lock);
    while(hnode->writer_here==1)
        pthread_cond_wait(&hnode->reader_can_enter,&hnode->lock);
    hnode->reader_count++;
    pthread_mutex_unlock(&hnode->lock);
}


static void read_unlock(hash_node_t *hnode)
{
    pthread_mutex_lock(&hnode->lock);
    hnode->reader_count--;
    if(hnode->reader_count!=0)
        pthread_cond_signal(&hnode->writer_can_enter);
    
    pthread_mutex_unlock(&hnode->lock);
}

static void write_lock(hash_node_t *hnode)
{
    pthread_mutex_lock(&hnode->lock);
    while((hnode->reader_count>0) || (hnode->writer_here==1))
        pthread_cond_wait(&hnode->writer_can_enter,&hnode->lock);
    hnode->writer_here=1;
    pthread_mutex_unlock(&hnode->lock);
}

static void write_unlock(hash_node_t *hnode)
{
    pthread_mutex_lock(&hnode->lock);
    hnode->writer_here=0;
    pthread_cond_signal(&hnode->reader_can_enter);
    pthread_cond_signal(&hnode->writer_can_enter);
    pthread_mutex_unlock(&hnode->lock);    
}

hash_table_t* hash_table_create( uint32_t size)
{
    hash_table_t *ht = NULL;
    int i = 0;

    if( size > 0 )
    {
        if((ht = malloc(sizeof(hash_table_t) + (sizeof(hash_node_t) * size))))
        {
            ht->size = size;

            for(i = 0; i <size; i++)
            {
                if((ht->table[i].tree = avl_create()) == NULL)
                {
                    TRACE(ERROR,"Failed to allocate momory");
                    
                    /* Allocation failure */
                    while(i>0)
                    {
                        avl_destroy(ht->table[--i].tree);
                    }
                    free(ht);
                    break;
                }
                else
                {
                    /* Initialize this Node */
                    ht->table[i].index = i;
                    if(read_write_lock_init(&ht->table[i]))
                    {
                        TRACE(ERROR,"Failed to allocate momory");
                         /* Allocation failure */
                        while(i>0)
                        {
                            avl_destroy(ht->table[--i].tree);
                        }
                        free(ht);
                        break;
                    }
                   
                }
            }
            
        }
        else
        {
            TRACE(ERROR,"Failed to allocate momory");
        }
    }
    else
    {
        TRACE(ERROR, "Invalid args");
    }

    return ht;
}
int hash_table_insert(hash_table_t *ht, cache_data_t* data,hash_data_node_t **new_node)
{
    uint32_t hash = 0;
    int ret = -1;
    avl_node_t* node = NULL;
    avl_tree_t* tree  = NULL;
    
    if( ht && data )
    {
        hash = cache_data_hash(data, ht->size);
        
        write_lock(&ht->table[hash]);
        tree = ht->table[hash].tree;
        if(( ret = avl_insert(tree, data, &node)) != -1)
        {
            if( new_node )
                *new_node = node;
        }
        
        write_unlock(&ht->table[hash]);
    }
    else
    {
        TRACE(ERROR,"Invalid args");
    }

    return ret;
}
hash_data_node_t* hash_table_search(hash_table_t *ht, cache_data_t* data)
{
    uint32_t hash = 0;
    avl_node_t* node = NULL;
    avl_tree_t* tree  = NULL;
    hash_data_node_t *dnode = NULL;
    
    if( ht && data )
    {
        hash = cache_data_hash(data, ht->size);
        tree = ht->table[hash].tree;
        read_lock(&ht->table[hash]);
        if(( node = avl_find(tree, data)))
        {
            dnode = node;
        }
        read_unlock(&ht->table[hash]);
    }
    else
    {
        TRACE(ERROR,"Invalid args");
    }

    return dnode;
}

void hash_table_destroy(hash_table_t *ht)
{
     int i = 0;

     if( ht )
     {
         TRACE(INFO,"Destroy");
         for(i = 0; i <ht->size; i++)
         {
             avl_destroy(ht->table[i].tree);
         }
         free(ht);
    }
    else
    {
        TRACE(ERROR,"Invalid args");
    }
}
