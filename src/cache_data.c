#include <stdio.h>
#include <inttypes.h>
#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>

#include "avl.h"
#include "cache_data.h"

#define MODULE "CacheData"
#include "trace.h"


static int min( int a, int b )
{
    return (a<b)?a:b;
}


int cache_data_cmpkey(cache_data_t* d1, cache_data_t* d2)
{
    int ret = 0;
    int len = 0;
    
    if( d1 && d2)
    {
        len = min(d1->key_len, d2->key_len);

        if((ret = memcmp(CACHE_KEY(d1), CACHE_KEY(d2), len )) == 0 )
        {
            ret = d1->key_len - d2->key_len;
        }
    }

    return ret;
}

void cache_data_free(cache_data_t *d)
{
    if( d )
    {
        free(d);
    }
}

cache_data_t* cache_data_alloc(uint32_t key_len, uint32_t val_len, uint8_t *key, uint8_t* val)
{
    cache_data_t *d = malloc(sizeof(cache_data_t) + key_len + val_len);

    if( d )
    {
        d->key_len = key_len;
        d->val_len = val_len;

        memcpy(CACHE_KEY(d), key, key_len);
        memcpy(CACHE_VAL(d), val, val_len);

        //printf("%d %d\n", key_len, val_len);
    }

    return d;
}

void cache_data_dump(cache_data_t *d)
{
    PRINT(DEBUG,"Key len : %d\t", d->key_len);
    PRINT(DEBUG,"val len : %d\n", d->val_len);
    
    HEXDUMP(DEBUG, "Key:",CACHE_KEY(d), d->key_len);
    
    HEXDUMP(DEBUG, "Value:",CACHE_VAL(d), d->val_len);
}
static uint32_t hash(uint8_t *data, uint32_t len)
{
    return data[0];
}

uint32_t cache_data_hash(cache_data_t * d, uint32_t size)
{
    uint32_t h = 0;

    if(d && size)
    {
        h = hash(CACHE_KEY(d), d->key_len);
        h%=size;
    }

    return h;
}

void cache_data_set(cache_data_t *d, uint8_t *extra, uint8_t extra_len , uint32_t* cas)
{
    if( d )
    {
        /* Copy Extra Info*/
        memcpy(d->cas, cas, sizeof(uint32_t)*2);
        memcpy(&d->flag, extra, sizeof(d->flag));
        memcpy(&d->expire, &extra[4], sizeof(d->expire));
        d->expire = ntohl(d->expire);
    }
}


void cache_data_get(cache_data_t *d, uint8_t *extra, uint8_t *extra_len , uint32_t* cas)
{
    if(d)
    {
        *extra_len = 4;
        memcpy(extra, &d->flag, sizeof(d->flag));
        memcpy(cas, d->cas, sizeof(uint32_t)*2);
    }
}

