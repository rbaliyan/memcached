#ifndef _CACHE_H_
#define _CACHE_H_

#include <inttypes.h>
#include "cache_data.h"
#include "hash_table.h"

typedef struct cache_s
{
    hash_table_t *ht;
} cachedb_t;

cachedb_t* cachedb_create(int hash_size);
int cachedb_get(cachedb_t *cachedb,  cache_data_t **centry, uint8_t *key, uint8_t *val, int key_len, int *val_len );
int cachedb_set(cachedb_t *cachedb, cache_data_t **centry, uint8_t *key, uint8_t *val, int key_len, int val_len  );

//int cachedb_invalidate(cachedb_t *cachedb);
void cachedb_destroy(cachedb_t *cachedb);
#endif
