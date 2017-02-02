#ifndef _CACHE_DATA_H_
#define _CACHE_DATA_H_

#include <inttypes.h>

#define CACHE_KEY(x)    (&((x)->data[0]))
#define CACHE_VAL(x)    (&((x)->data[(x)->key_len]))

#define CACHE_EXTRA_LEN 8

typedef struct cache_data_s
{
    uint32_t key_len;
    uint32_t val_len;
    uint32_t flag;
    uint32_t expire;
    uint32_t cas[2];        
    uint8_t data[0];
} cache_data_t;

int cache_data_cmpkey(cache_data_t* d1, cache_data_t* d2);
cache_data_t* cache_data_alloc(uint32_t key_len, uint32_t val_len, uint8_t *key, uint8_t *val);
void cache_data_free(cache_data_t *d);
void cache_data_dump(cache_data_t *d);
uint32_t cache_data_hash(cache_data_t * d, uint32_t size);
void cache_data_set(cache_data_t *d, uint8_t *extra, uint8_t extra_len , uint32_t* cas);
void cache_data_get(cache_data_t *d, uint8_t *extra, uint8_t *extra_len , uint32_t* cas);
#endif
