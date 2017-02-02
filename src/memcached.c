#include <stdio.h>
#include <string.h>
#include <malloc.h>
#include <pthread.h>
#include <unistd.h>
#include "memcached.h"

#define MODULE "Memcached"
#include "trace.h"

void dump_req(memcached_req_t *req)
{
    PRINT(DEBUG,"Magic : %02x\n", req->magic);
    PRINT(DEBUG,"opcode : %02x\n", req->opcode);
    PRINT(DEBUG,"keylen : %d\n", req->key_len);
    PRINT(DEBUG,"extra_len : %d\n", req->extra_len);
    PRINT(DEBUG,"len : %d\n", req->len);
    //HEXDUMP(DEBUG,"body",req->data, req->len);
}

void dump_rsp(memcached_rsp_t *rsp)
{
    PRINT(DEBUG,"Magic : %02x\n", rsp->magic);
    PRINT(DEBUG,"opcode : %02x\n", rsp->opcode);
    PRINT(DEBUG,"keylen : %d\n", rsp->key_len);
    PRINT(DEBUG,"extra_len : %d\n", rsp->extra_len);
    PRINT(DEBUG,"len : %d\n", rsp->len);
    PRINT(DEBUG,"status : %d\n", rsp->status);
    //HEXDUMP(DEBUG,"body",rsp->data, rsp->len);
}

static void ntoh_req(memcached_req_t* req)
{
    req->key_len = ntohs(req->key_len);
    req->len = ntohl(req->len);
}


static void hton_rsp(memcached_rsp_t* rsp)
{
    rsp->key_len = htons(rsp->key_len);
    rsp->len = htonl(rsp->len);
    rsp->status = htons(rsp->status);
}

static int validate(memcached_req_t* req, buffer_t* buffer )
{
    int ret = 0;
    switch(req->opcode)
    {
        case MCACHE_OPCODE_GET:
            if(buffer->req_len < (sizeof(memcached_req_t) + req->len ))
            {
                TRACE(DEBUG,"Length Mismatch %d: %lu",buffer->req_len, (sizeof(memcached_req_t) + req->len ));
                ret = -2;
            }
            else if( req->key_len == 0 )
            {
                TRACE(DEBUG,"Key len is zero");
                ret = -2;
            }
            break;
        case MCACHE_OPCODE_SET:
            if(buffer->req_len < (sizeof(memcached_req_t) + req->len ))
            {
                TRACE(DEBUG,"Length Mismatch %d: %lu",buffer->req_len, (sizeof(memcached_req_t) + req->len ));
                ret = -2;
            }
            else if( req->key_len == 0)
            {
                TRACE(DEBUG,"Key len is zero");
                ret = -2;
            }
            else if(req->extra_len == 0 )
            {
                TRACE(DEBUG,"Val len is zero");
                ret =-2;
            }
            break;
        default:
            TRACE(DEBUG,"Unsupported cmd");
            ret = -1;
            break;
    }
    return ret;
}

static int process(memcached_t *memcached, memcached_req_t* req, memcached_rsp_t *rsp)
{
    cache_data_t *centry = NULL;
    int status = 0;
    int val_len = 0;
    rsp->magic = MCACHE_RSP_MAGIC;
    rsp->opcode = req->opcode;
    rsp->data_type = MCACHE_DATA_TYPE;
    uint8_t *val, *key;
    switch(req->opcode)
    {
        case MCACHE_OPCODE_GET:
            
            val_len = memcached->max_val_len;
            rsp->extra_len = 4;
            val = MCACHE_GET_RSP_VAL(rsp);
            key = MCACHE_GET_REQ_KEY(req);
            HEXDUMP(DEBUG,"Find Key :",key, req->key_len);
            if((cachedb_get(memcached->cache, &centry, key,val, req->key_len, &val_len )) == 0)
            {
                TRACE(DEBUG,"Found Value");
                HEXDUMP(DEBUG,"Found Value :",val, val_len);
                cache_data_get(centry, MCACHE_GET_RSP_EXTRA(rsp), &rsp->extra_len, rsp->cas);
                rsp->status = MCACHE_STATUS_SUCCESS;
                rsp->len = val_len + rsp->extra_len;
                dump_rsp(rsp);
            }
            else
            {
                TRACE(DEBUG,"Key Not Found");
                rsp->status = MCACHE_STATUS_NOT_FOUND;
                rsp->len = 0;
                rsp->extra_len = 0;
            }
            break;
        case MCACHE_OPCODE_SET:
            rsp->len = 0;
            rsp->extra_len = 0;
            val_len = req->len - req->key_len - req->extra_len;
            if(( status =  cachedb_set(memcached->cache, &centry, MCACHE_SET_REQ_KEY(req), MCACHE_SET_REQ_VAL(req), req->key_len, val_len  )) == 0 )
            {
                TRACE(DEBUG,"Set Done");
                rsp->status = MCACHE_STATUS_SUCCESS;
                memcpy(rsp->cas, req->cas, 8);
                dump_rsp(rsp);
            }
            else
            {
                TRACE(DEBUG,"Set Failed");
                rsp->status = MCACHE_STATUS_TOO_LARGE;
            }
        
            break;
        case MCACHE_OPCODE_QUIT:
            /* Close connection */
            return 2;
            break;
        default:
            
            break;
    }
    return 0;
}

static void *memcached_main_task( void *args )
{
    memcached_t *memcached = (memcached_t*)args;
    buffer_t *buffer= NULL;
    memcached_req_t *req = NULL;
    memcached_rsp_t *rsp = NULL;
    int len = 0;
    int ret = 0;
    
    while(memcached->state == MCACHE_STATE_RUNNING)
    {
        /* Get New Available buffer */
        if(( buffer != NULL ) || (buffer = server_buffer_get(memcached->server)))
        {
            /* Read Bufer Header */
            len = server_buffer_read_bytes( memcached->server, buffer, sizeof(memcached_req_t));
            if( len < sizeof(memcached_req_t))
            {
                /* Read Header Failed Close Connection and release buffer */
                TRACE(ERROR,"Failed to read Header : %d", len);
                
                server_buffer_release(memcached->server, buffer);
                
                buffer = NULL;
                
                continue;
            }
            
                
            TRACE(DEBUG,"buffer recvd");
            HEXDUMP(DEBUG,"Req buffer", buffer->req, len);
            
            req = (memcached_req_t*)buffer->req;
            rsp = (memcached_rsp_t*)buffer->rsp;
            
            /* Deserialize */
            ntoh_req(req);
            
            TRACE(DEBUG, "Rest of Bytes : %d", req->len);
            dump_req(req);
            
            /* Read Message Body */
            if(( req->len >0 ) && ( len = server_buffer_read_bytes(memcached->server,buffer, req->len)) < req->len )
            {
                /* Close connection and release buffer */
                TRACE(ERROR,"Failed to read Header : %d", len);
                
                server_buffer_release(memcached->server, buffer);
                
                buffer = NULL;
                
                continue;
            }
            else if( req->len >0 )
            {
                /* Req Data received */
                HEXDUMP(DEBUG,"Req buffer", buffer->req, req->len);
            }
            
            /* Validate */
            if((validate(req, buffer))==0)
            {   
                /* Process request */
                if(( ret = process(memcached, req, rsp )))
                {
                    /* Failure Process */
                    TRACE(ERROR,"failure processing");

                    /* Close Socket */
                    server_buffer_release(memcached->server, buffer);
                    
                    buffer = NULL;
                }
                else if( ret == 2 )
                {
                    /* Close Socket Request */
                    server_buffer_release(memcached->server, buffer);
                    
                    buffer = NULL;
                    
                    continue;
                }
                else
                {
                    /* Process done prepare response */
                    buffer->rsp_len = sizeof(memcached_rsp_t) + rsp->len;
                    
                    TRACE(DEBUG,"Response length : %d", buffer->rsp_len);
                }
            }
            else
            {
                TRACE(ERROR,"Validation failed");
                buffer->rsp_len = 0;
            }
            
            /* If there is any data to send then send */
            if( buffer->rsp_len > 0 )
            {
                dump_rsp(rsp);
                hton_rsp(rsp);
                
                HEXDUMP(DEBUG,"Rsp buffer", buffer->rsp, buffer->rsp_len); 
                if((ret = server_buffer_send(memcached->server, buffer)) <= 0 )
                {
                    if( ret < 0 )
                    {
                        /* Send failed release buffer */
                        TRACE(ERROR,"Failed to send reply");
                    }
                    
                    server_buffer_release(memcached->server, buffer);
                    
                    buffer = NULL;
                    
                    continue;
                }            
            }
            
            /* Reuse same buffers, until connection is closed by remote */
        }
    }

    
    pthread_exit(NULL);
}

/*
 * Set Maximum Key Len and Val Len, This decides the request and response buffer size
 * 
 */ 
int memcached_max_key_val(memcached_t *memcached, int key_len, int val_len)
{
    int ret = -1;
    
    if(memcached)
    {
        if(key_len > 0)
            memcached->max_key_len = key_len;        
        
        if(val_len > 0)
            memcached->max_val_len = val_len;
        
        ret = server_set_buffer_size(memcached->server, MCACHE_MAX_REQ_SIZE(memcached), MCACHE_MAX_RSP_SIZE(memcached));
    }
        
    return ret;
}

memcached_t* memcached_init(server_t *server, int thread_count, int hash_size)
{
    memcached_t *memcached = NULL;
    if(( thread_count > 0 ) && (hash_size > 0))
    {
        if((memcached = malloc(sizeof(memcached_t))))
        {
            memcached->max_key_len = MCACHE_KEY_LEN_DEFAULT;
            memcached->max_val_len = MCACHE_VAL_LEN_DEFAULT;
        
            if(server_set_buffer_size(server,MCACHE_MAX_REQ_SIZE(memcached), MCACHE_MAX_RSP_SIZE(memcached)))
            {
                TRACE(ERROR,"Failed to set buffer size");
            }
            if((memcached->tid = calloc(sizeof(pthread_t), thread_count )))
            {
                if(( memcached->cache  = cachedb_create(hash_size)))
                {
                    memcached->state = MCACHE_STATE_INIT;
                    memcached->tcount = thread_count;
                    memcached->server = server;
                }
                else
                {
                    TRACE(ERROR,"failed to create hashtable\n");
                    free(memcached->tid);
                    free(memcached);
                    memcached = NULL;
                }
            }
            else
            {
                TRACE(ERROR,"failed to allocate memory\n");
                free(memcached);
                memcached = NULL;
            }
        }
        else
        {
            TRACE(ERROR,"failed to allocate memory\n");
        }
        
    }
    else
    {
        TRACE(ERROR,"Invalid args");
    }

    return memcached;
}

/*
 * Create Thread Pool to handle incoming requests
 * 
 */ 
int memcached_start( memcached_t *memcached )
{
    int i;
    int ret = 0;
    
    if( memcached && memcached->state == MCACHE_STATE_INIT)
    {
        memcached->state = MCACHE_STATE_RUNNING;
        
        for( i = 0; i < memcached->tcount; i++ )
        {
            if((ret = pthread_create( &memcached->tid[i], NULL, memcached_main_task, (void*) memcached)))
            {
                memcached->state = MCACHE_STATE_INIT;
                TRACE(ERROR,"Failed to create thread");
                ret = -1;
            }
        }

        if( ret == 0 )
        {
            TRACE(DEBUG,"Start Server");
            ret = server_start(memcached->server,10);
        }
    }
    else
    {
        TRACE(ERROR,"Invalid args");
    }

    return ret;
}

int memcached_shutdown(memcached_t *memcached )
{
    int i;
    int ret = -1;
    if(memcached)
    {
        TRACE(INFO,"Start Shutdown");          
        if( memcached->state == MCACHE_STATE_RUNNING )
        {
            /* Begin Shutdown */
            memcached->state = MCACHE_STATE_STOPPED;

            TRACE(INFO,"Shutdown server");
            if(server_shutdown(memcached->server) != 0 )
            {
                TRACE(ERROR,"Failed to shutdown server");
            }
            else
            {
                ret = 0;
            }
            
            TRACE(INFO,"Wait for threads to exit");
            
            /* Clean Up */
            for(i=0;i<memcached->tcount; i++)
            {
                pthread_join(memcached->tid[i], NULL);
            }
            
            TRACE(INFO,"All threads exited");
        }
        
        
        TRACE(INFO,"Shutdown complete");
    }
    
    return ret;

}

void memcached_destroy(memcached_t *memcached)
{
    if(memcached && (memcached->state != MCACHE_STATE_NULL))
    {
        TRACE(INFO,"Destroy start");
        if(memcached->state == MCACHE_STATE_RUNNING)
            memcached_shutdown(memcached);
            
        if(memcached->tid)
            free(memcached->tid);
            
        memcached->tid = NULL;
        cachedb_destroy(memcached->cache);
        memcached->cache = NULL;
        
        TRACE(INFO,"Destroy Server");
        server_destroy(memcached->server);
        
        free(memcached);
        
        TRACE(INFO,"Destroyed");
    }
}
