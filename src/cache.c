#include <malloc.h>
#include <stdio.h>
#include <string.h> 
#include "cache.h"
#include "hash_table.h"
#include "cache_data.h"

#define MODULE "Cachedb"
#include "trace.h"

#define MIN(a,b)    ((a)<(b)?(a):(b))

cachedb_t* cachedb_create(int hash_size)
{
    cachedb_t *cdb = NULL;
    
    if( hash_size > 0 )
    {
        hash_table_init();
        
        if((cdb = malloc(sizeof(cachedb_t))))
        {        
            if(( cdb->ht = hash_table_create(hash_size)) == NULL)
            {
                free(cdb);
                cdb = NULL;
            }
        }
        else
        {
            TRACE(ERROR,"Memory allocation failure");
        }
    }
    else
    {
        TRACE(ERROR,"Invalid args");
    }

    return cdb;
}

int cachedb_get(cachedb_t *cachedb,  cache_data_t **centry, uint8_t *key, uint8_t *val, int key_len, int *val_len )
{
    int ret = -1;
    cache_data_t *creq = NULL;
    cache_data_t *found = NULL;
    hash_data_node_t *node;
    
    uint8_t *ptr;
    
    if(cachedb && key && key_len > 0)
    {
        HEXDUMP(DEBUG,"key", key, key_len);
        
        if((creq = cache_data_alloc(key_len, 0, key, NULL)))
        {
            if((node=hash_table_search(cachedb->ht, creq)))
            {
                found = hash_get_cache(node);
                if(val_len && val)
                {
                    TRACE(DEBUG,"Avail Buffer : %d, need : %d", *val_len, found->val_len);
                    *val_len = MIN(*val_len, found->val_len);
                    if(val && *val_len > 0)
                    {
                        ptr = CACHE_VAL(found);
                        memcpy(val, ptr,*val_len);
                        HEXDUMP(DEBUG,"Value", ptr, found->val_len);
                        HEXDUMP(DEBUG,"Value", val, *val_len);
                    }
                    *val_len = found->val_len;
                }
                else
                {
                    TRACE(DEBUG,"Value not requested");
                }

                if(centry)
                    *centry = found;
                ret = 0;
            }
            else
            {
                TRACE(INFO,"Key Not Found.");
                ret = 1;
            }
            
            cache_data_free(creq);
        }
        else
        {
            TRACE(ERROR,"Memory allocation");
            ret = -2;
        }
    }
    else
    {
        TRACE(ERROR,"Invalid args");
    }
    return ret;
}



int cachedb_set(cachedb_t *cachedb, cache_data_t **centry, uint8_t *key, uint8_t *val, int key_len, int val_len  )
{
    int ret = -1;
    cache_data_t *c = NULL;
    hash_data_node_t *node;
    int status;
    
    if(cachedb && key && key_len > 0)
    {
        if((c = cache_data_alloc(key_len, val_len, key, val)))
        {
            if((status=hash_table_insert(cachedb->ht, c, &node)) != -1 )
            {
                if(centry)
                    *centry = hash_get_cache(node);
                if(status == 1)
                {
                    TRACE(INFO,"Duplicate Entry");
                }
                
                ret = 0;
            }
            else
            {
                TRACE(ERROR,"Memory allocation failure");
                ret = -1;
            }
        }
        else
        {
            TRACE(ERROR,"Memory allocation failure");
            ret = -2;
        }
    }
    else
    {
        TRACE(ERROR,"Invalid args");
    }

    return ret;
}

void cachedb_destroy(cachedb_t *cachedb)
{
    if(cachedb)
    {
        TRACE(INFO,"Destroy");
        hash_table_destroy(cachedb->ht);
        cachedb->ht = NULL;
        free(cachedb);
    }
    else
    {
        TRACE(ERROR,"Invalid args");
    }
}
