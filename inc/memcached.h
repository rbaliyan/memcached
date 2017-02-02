#ifndef _EmemcachedD_H
#define _MEmemcachedD_H

#include <pthread.h>
#include "cache.h"
#include "server.h"

#define MCACHE_REQ_HEADER_SIZE 24
#define MCACHE_RSP_HEADER_SIZE 24

#define MCACHE_GET_REQ_KEY(x)   (&((x)->data[0]))
#define MCACHE_SET_REQ_KEY(x)   (&((x)->data[(x)->extra_len]))
#define MCACHE_SET_REQ_VAL(x)   (&((x)->data[(x)->extra_len + (x)->key_len]))


#define MCACHE_GET_RSP_VAL(x)   (&((x)->data[(x)->extra_len]))
#define MCACHE_GET_RSP_EXTRA(x)   (&((x)->data[0]))

#define MCACHE_DATA_TYPE 0x00  /* RAW Byte */

#define MCACHE_REQ_MAGIC 0x80
#define MCACHE_RSP_MAGIC 0x81

#define MCACHE_KEY_LEN_DEFAULT 128
#define MCACHE_VAL_LEN_DEFAULT 128

#define MCACHE_EXTRA_MAX_SIZE  0x08

#define MCACHE_MAX_BODY_SIZE(m) ((m)->max_key_len+(m)->max_val_len + MCACHE_EXTRA_MAX_SIZE)

#define MCACHE_MAX_REQ_SIZE(m)  (sizeof(memcached_req_t) + MCACHE_MAX_BODY_SIZE(m))

#define MCACHE_MAX_RSP_SIZE(m)  (sizeof(memcached_rsp_t) + MCACHE_MAX_BODY_SIZE(m))

enum
{
    MCACHE_OPCODE_GET   = 0x00,
    MCACHE_OPCODE_SET   = 0x01,
    MCACHE_OPCODE_QUIT  = 0x07, 
};
enum
{
    MCACHE_STATE_NULL,
    MCACHE_STATE_INIT,
    MCACHE_STATE_RUNNING,
    MCACHE_STATE_STOPPED,
};
enum
{
    MCACHE_STATUS_SUCCESS,
    MCACHE_STATUS_NOT_FOUND,
    MCACHE_STATUS_EXISTS,
    MCACHE_STATUS_TOO_LARGE,
    MCACHE_STATUS_INVALID_ARGS,
    MCACHE_STATUS_NOT_STORED,
    MCACHE_STATUS_NON_NUMERIC,
};

typedef struct memcached_req_s
{
    uint8_t magic;
    uint8_t opcode;
    uint16_t key_len;
    uint8_t extra_len;
    uint8_t data_type;
    uint16_t reserved;
    uint32_t len;
    uint32_t opaque;
    uint32_t cas[2];
    uint8_t data[0];
} memcached_req_t;


typedef struct memcached_resp_s
{
    uint8_t magic;
    uint8_t opcode;
    uint16_t key_len;
    uint8_t extra_len;
    uint8_t data_type;
    uint16_t status;
    uint32_t len;
    uint32_t opaque;
    uint32_t cas[2];
    uint8_t data[0];
} memcached_rsp_t;

typedef struct
{
    int state;
    int tcount;
    int max_key_len;
    int max_val_len;
    pthread_t *tid;
    server_t *server;
    cachedb_t *cache;
} memcached_t;

memcached_t* memcached_init(server_t *server, int thread_count, int hash_size);
int memcached_max_key_val(memcached_t *memcached, int key_len, int val_len);
int memcached_start( memcached_t *memcached);
int memcached_shutdown(memcached_t *memcached );
void memcached_destroy(memcached_t *memcached);

#endif
