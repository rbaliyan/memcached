#include <netdb.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <malloc.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <pthread.h>
#include "server.h"

#define MODULE "Server"
#include "trace.h"

/*
 * Get Buffer with given state 
 * Not be called directly must be protected by mutex
 * Performance can be increased by maintaining different queues 
 */
static int get_buffer(server_t *server, int state)
{
    int i;
    for(i = 0; i < server->max_conn; i++)
    {
        if(server->buffers[i].state == state)
            break;
    }

    return (i<server->max_conn) ? i : -1;
}

/*
 * Add buffer in queue and Signal threads which is waiting for it
 * If Buffer is freed the signal to main thread that buffer is freed
 * If Buffer is filled then signal consumers threads that new data is available
 */ 
static void add_buffer(server_t  *server, buffer_t *buffer, int state)
{
    pthread_mutex_lock(&server->lock);
    buffer->state = state;
    if( state == BUFFER_STATE_FREE)
        pthread_cond_signal(&server->freed);
    else if( state == BUFFER_STATE_USED)
        pthread_cond_signal(&server->available);
    pthread_mutex_unlock(&server->lock);
}

/* 
 * Fetch a buffer with requested state and wait until it is done
 * 
 */ 
static buffer_t* get_buffer_wait(server_t *server, int state)
{
    int index = 0;
    buffer_t *buffer = NULL;
    pthread_mutex_lock(&server->lock);
    index = get_buffer(server, state);
    if(index == -1)
    {
        if( state == BUFFER_STATE_FREE)
            pthread_cond_wait(&server->freed, &server->lock);
        else if( state == BUFFER_STATE_USED)
            pthread_cond_wait(&server->available, &server->lock);
        index = get_buffer(server, state);
    }
    if( index != -1 )
    {
        buffer = &server->buffers[index];

        /* Set Buffer State */
        if( state == BUFFER_STATE_FREE)
            buffer->state = BUFFER_STATE_LOCKED;
        else if( state == BUFFER_STATE_USED)
            buffer->state = BUFFER_STATE_PROCESS;
    }

    pthread_mutex_unlock(&server->lock);

    return buffer;
}

/*
 * Init buffer to be used by server
 */ 
static int buffer_init(server_t *server, buffer_t *buffer)
{
    int ret = 0;
    if( buffer->req == NULL )
    {
        if((buffer->req = malloc(server->max_req_size)) == NULL )
        {
            TRACE(ERROR,"Failed to allocate memory");
            return -1;
        }
    }
    else if(buffer->req_size != server->max_req_size)
    {
        if((buffer->req = realloc(buffer->req, server->max_req_size)) == NULL )
        {
            TRACE(ERROR,"Failed to allocate memory");
            return -1;
        }        
    }
    
    if( buffer->rsp == NULL )
    {
        if((buffer->rsp = malloc(server->max_rsp_size)) == NULL )
        {
            TRACE(ERROR,"Failed to allocate memory");
            return -1;
        }
    }
    else if(buffer->rsp_size != server->max_rsp_size)
    {
        if((buffer->rsp = realloc(buffer->rsp, server->max_rsp_size)) == NULL )
        {
            TRACE(ERROR,"Failed to allocate memory");
            return -1;
        }
    }
        
    /* Buffer Size */
    buffer->req_size = server->max_req_size;
    buffer->rsp_size = server->max_rsp_size;
    /*TRACE(DEBUG, "New Buffers Size Req : %d, Rsp :%d", buffer->req_size,buffer->rsp_size);*/

    memset(&buffer->addr, 0, sizeof(buffer->addr));
    memset(buffer->req, 0, buffer->req_size);
    memset(buffer->rsp, 0, buffer->rsp_size);
    buffer->req_len = buffer->rsp_len = 0;

    return ret;
}
/*
 * Main Server Thread
 * 
 * For TCP Connection :
 *                      This Thread only accepts new connections and then 
 *     add the buffer in the queue
 *     Incoming data is read by servicing thread 
 * 
 * For UDP Connection :
 *                      This Thread read entire data and then add in used queue
 */ 
static void *server_main_task( void * args)
{
    pthread_t tid = pthread_self();
    server_t *server = (server_t*)args;
    buffer_t *buffer = NULL;
    unsigned int slen;
    char str[INET_ADDRSTRLEN];
    struct timeval tv;
    tv.tv_sec = 0;
    tv.tv_usec = RECV_TIMEOUT_MS*1000;

    if( server )
    {
        TRACE(DEBUG,"Server Thread : %lu", tid);
        while( server->state == SERVER_STATE_RUNNING )
        {
            if( buffer == NULL )
            {
                TRACE(DEBUG,"Get New Buffer");
                /* Wait for new avail index */
                while((( buffer = get_buffer_wait( server, BUFFER_STATE_FREE )) == NULL ) && 
                       ( server->state == SERVER_STATE_RUNNING ));
                if(buffer == NULL)
                        break;  /* Server Shutdown */
                    
            }
            if( buffer_init(server, buffer) )
            {
                TRACE(ERROR,"Failed to allocate memory");
            }
            slen = sizeof(buffer->addr);
            if(server->udp)
            {
                /*TRACE(DEBUG, "RECV on buffer : %d , %d", buffer->index, buffer->req_size);*/
                if((buffer->req_len = recvfrom(server->sock, buffer->req, buffer->req_size, 0, (struct sockaddr *)&buffer->addr, &slen)) < 0)
                {
                    if((errno == EAGAIN) || (errno == EWOULDBLOCK))
                        buffer->req_len = 0;
                    else
                        TRACE(ERROR,"Failed recvfrom %d: %s", errno, strerror(errno));
                }
                else if( buffer->req_len > 0)
                {
                    inet_ntop(AF_INET, &(buffer->addr.sin_addr), str, INET_ADDRSTRLEN);
                    TRACE(DEBUG,"Connection Request from> %s:%d", str, ntohs(buffer->addr.sin_port));

                    TRACE(DEBUG,"Recvd %d bytes", buffer->req_len);
                    
                    add_buffer(server, buffer, BUFFER_STATE_USED );

                    buffer = NULL;
                }            
            }
            else
            {
                /* TCP Connection, Accept New Connection and thread will read data itself */
                slen = sizeof(buffer->addr);
                if((buffer->sock = accept(server->sock, (struct sockaddr*)&buffer->addr, &slen)) < 0 )
                {
                    /* some Error */
                }
                else
                {
                    /* Make Sure Threads are not blocked when shutdown signal is received */
                    if (setsockopt(server->sock, SOL_SOCKET, SO_RCVTIMEO,&tv,sizeof(tv)) < 0) 
                    {
                        TRACE(ERROR,"Failed setsockopt : %s", strerror(errno));
                    }
                    
                    /* New Connection Created */
                    add_buffer(server, buffer, BUFFER_STATE_USED );
                    buffer = NULL;
                }
            }

        }
        
        if(buffer)
        {
            add_buffer(server, buffer, BUFFER_STATE_FREE );
        }
    }

    pthread_exit(NULL);
}


/*
 * 
 * Initialize Server Module
 * 
 */ 
server_t* server_init(short port, uint32_t  max_conn, int udp)
{
    server_t *server = NULL;
    struct timeval tv;
    int i=0;
    int stype = SOCK_STREAM;

    if( max_conn  && port )
    {
        tv.tv_sec = 0;
        tv.tv_usec = RECV_TIMEOUT_MS*1000;
        
        
        if((server = (server_t*)malloc(sizeof(server_t) + (sizeof(buffer_t) * max_conn))))
        {
            memset(&server->addr, 0, sizeof(server->addr));
            server->addr.sin_addr.s_addr = htonl(INADDR_ANY);
            server->addr.sin_port = htons(port);
            server->max_conn = max_conn;
            server->sock = -1;
            server->udp = udp;
            
            if(udp)
                stype = SOCK_DGRAM;

            if((server->sock = socket(AF_INET, stype, 0)) < 0 )
            {
                free(server);
                TRACE(ERROR,"Socket failed : %s", strerror(errno));
                server = NULL;

                return server;
            }
            else if (setsockopt(server->sock, SOL_SOCKET, SO_RCVTIMEO,&tv,sizeof(tv)) < 0) 
            {
                TRACE(ERROR,"Failed setsockopt : %s", strerror(errno));
            }
            else if((bind(server->sock, (struct sockaddr *)&server->addr, sizeof(server->addr))) != 0)
            {
                close(server->sock);
                free(server);
                TRACE(ERROR,"Failed Bind : %s", strerror(errno));
                server = NULL;
                return server;
            }
            else if (pthread_mutex_init(&server->lock, NULL) != 0)
            {
                TRACE(ERROR,"Failed to Mutex Init : %s", strerror(errno));
                close(server->sock);
                free(server);
                server = NULL;
                return server;
            }
            else if( pthread_cond_init(&server->freed, NULL) != 0 )
            {
                TRACE(ERROR,"Failed to Mutex Init : %s", strerror(errno));
                close(server->sock);
                pthread_mutex_destroy(&server->lock);
                free(server);
                server = NULL;
                return server;
            }
            else if(pthread_cond_init(&server->available, NULL) != 0)
            {
                TRACE(ERROR,"Failed to Mutex Init : %s", strerror(errno));
                close(server->sock);
                pthread_mutex_destroy(&server->lock);
                pthread_cond_destroy(&server->freed);
                free(server);
                server = NULL;
                return server;
            }

            for(i=0;i<max_conn;i++)
            {
                server->buffers[i].index = i;
                server->buffers[i].state = BUFFER_STATE_FREE;
            }

            /* Server is ready */
            server->state = SERVER_STATE_INIT;
            TRACE(DEBUG, "Server Ready listen Port : %d", port);
        }
        else
        {
            TRACE(ERROR,"Memory allocation failed");
        }
    }
    else
    {
        TRACE(ERROR,"Invalid args");
    }

    return server;
}

/* 
 * Start Server Thread
 * 
 */ 
int server_start(server_t *server, int backlog )
{
    int ret = -1;
    if( server && (server->state == SERVER_STATE_INIT) && (backlog > 0))
    {
        if(( server->udp == 0 )&& (ret = listen(server->sock, backlog)))
        {
            TRACE(ERROR,"Failed listen : %s", strerror(errno));
        }
        else
        {
            TRACE(DEBUG,"Server Starting");
            server->state = SERVER_STATE_RUNNING;
            if((ret = pthread_create( &server->tid, NULL, server_main_task, (void*) server)))
            {
                server->state = SERVER_STATE_INIT;
               TRACE(ERROR,"Failed thread create : %s", strerror(errno));
            }
            else
            {
                TRACE(DEBUG,"Server Thread Created : %lu", server->tid);
                ret = 0;
            }
        }
    }
    else
    {
        TRACE(ERROR,"invalid args");
    }

    return ret;
}

/*
 * Read data byte on requested connection and fill buffer
 * 
 */ 
int server_buffer_read_bytes(server_t *server, buffer_t *buffer, uint32_t size)
{
    int len = 0, n = 0;
    uint32_t offset = 0;

    if( server  && buffer )
    {
        if(server->udp == 0)
        {
            offset = buffer->req_len;
            
            TRACE(DEBUG,"Read %d bytes", size);
            
            while(( len < size ) && (server->state == SERVER_STATE_RUNNING))
            {
                if((n = recv(buffer->sock, &buffer->req[offset], size, 0)) > 0)
                    len += n;
                else if((errno == EAGAIN) || (errno == EWOULDBLOCK))
                    n = 0;
                else
                    break; /* Error */
            }
            
            if(server->state != SERVER_STATE_RUNNING)
            {
                buffer->closed = 1;
                len = -1;
            }
            else if( n < 0 )
            {
                TRACE(ERROR,"Error Close Socket");
                
                buffer->closed = 1;

                len = -1;
            }
            else if( n == 0)
            {
                /* Connection is closed */
                buffer->closed = 1;
            }
            else
            {
                /* Some Data received update req len */
                buffer->req_len+=len;
            }
        
        }
        else
        {
            /* For UDP all data read is done by main thread*/
            if( size < buffer->req_len)
                len = size;
        }
        
        TRACE(DEBUG, "Total Recvd : %d bytes, %d", len);
    }
    
    return len;
}

/*
 * Connection is closed release the buffer in queue 
 * 
 */ 
void server_buffer_release(server_t *server, buffer_t* buffer)
{
    if( server && buffer )
    {
        TRACE(DEBUG,"Release Buffer : %d", buffer->index);
        
        buffer->req_len = 0;
        
        close(buffer->sock);
        buffer->sock = -1;
        buffer->req_len = 0;
        buffer->rsp_len = 0;
        
        add_buffer(server, buffer, BUFFER_STATE_FREE );
    }
}

/*
 * Get used vailable buffer from the queue
 */ 
buffer_t* server_buffer_get(server_t *server)
{
    buffer_t *buffer = NULL;
    if( server )
    {
        /* Wait for buffer */
        while((( buffer  = get_buffer_wait(server, BUFFER_STATE_USED)) == NULL ) && 
               ( server->state == SERVER_STATE_RUNNING ));
        
        if( buffer )
        {
            if( server->udp == 0)
            {
                /* Read Data explicityly */
                buffer->req_len = 0;
                
            }
            else
            {
                /* For UDP connection is already closed */
                buffer->closed = 1;
            }
        }
        
        
        TRACE(DEBUG,"Process Buffer : %d", buffer ? buffer->index : -1);
    }
    return buffer;
}

/*
 * 
 * Send Data to remote Host
 * 
 */
int server_buffer_send(server_t *server, buffer_t* buffer)
{
    unsigned int slen;
    int len, n;
    if( server && buffer )
    {
        if(server->state == SERVER_STATE_RUNNING)
        {
            if( buffer->rsp && buffer->rsp_len > 0)
            {
                if( server->udp )
                {
                    /* UDP Send Data */
                    slen = sizeof(buffer->addr);
                    if(( sendto(server->sock, buffer->rsp, buffer->rsp_len, 
                         0, (struct sockaddr *)&buffer->addr, slen)) < 0 )
                    {
                        TRACE(ERROR,"Failed sendto : %s", strerror(errno));
                        
                        len = -1;
                    }
                    else 
                    {
                        len = buffer->rsp_len;
                    }
                }
                else
                {
                   /* TCP Send Data */
                   len = 0;
                   
                   while(( len < buffer->rsp_len ) && ( server->state == SERVER_STATE_RUNNING ))
                   {
                       if(( n = send(buffer->sock, &buffer->rsp[len], buffer->rsp_len, 0)) > 0 )
                        len += n;
                       else
                           break;
                   }
                   
                   if(server->state != SERVER_STATE_RUNNING)
                       len = -1;
                   else if(buffer->closed)
                   {
                      len = 0;
                   }
                   
                }
            }
            else
            {
                len = -1;
            }
        }
        else
        {
            len = -2;
        }
        
        /* Reset Buffer Request */
        buffer->rsp_len = 0;        
        buffer->req_len = 0;

        TRACE(DEBUG,"Reply Sent for Buffer : %d", buffer->index);
    }
    else
    {
        TRACE(ERROR,"incalid args");
        len = -1;
    }
    
    return len;
}

int server_shutdown(server_t *server)
{
    int ret = -1;
    if(server && (server->state == SERVER_STATE_RUNNING))
    {
        TRACE(INFO,"Shutdown Server");
        server->state = SERVER_STATE_STOPPED;
        
        pthread_cond_signal(&server->freed);
        pthread_cond_signal(&server->available);
        
        pthread_join(server->tid, NULL);
        
        /* Server Task Down */
        close(server->sock);
        
        TRACE(INFO,"Shutdown Server Complete");
        
        ret = 0;
    }
    
    return ret;
}

int server_set_buffer_size(server_t *server, int req_size, int rsp_size)
{
    int ret = -1;
    if(server)
    {
        if(req_size > 0)
        {
            server->max_req_size = req_size;
            ret = 0;
        }
            
        
        if(rsp_size > 0)
        {
            server->max_rsp_size = rsp_size;
            ret = 0;
        }
        
        TRACE(DEBUG,"New Sizes : Req : %d, Rsp : %d", server->max_req_size, server->max_rsp_size);
    }
    else
    {
        TRACE(ERROR,"Invalid args");
    }
    
    return ret;
}
void server_destroy(server_t *server)
{
    int i = 0;
    if(server && (server->state != SERVER_STATE_NULL))
    {
        TRACE(INFO,"Destroy Start");
        if(server->state == SERVER_STATE_RUNNING)
            server_shutdown(server);
        
        pthread_mutex_destroy(&server->lock);
        pthread_cond_destroy(&server->freed);
        pthread_cond_destroy(&server->available);
        
        TRACE(INFO,"Wait for buffers to be freed");
        for(i=0;i<server->max_conn;i++)
        {
            while((server->buffers[i].state != BUFFER_STATE_FREE ) && (server->buffers[i].state != BUFFER_STATE_USED))
            {
                TRACE(INFO,"Waiting for buffer %d to free",i);
            }
            if(server->buffers[i].req)
                free(server->buffers[i].req);
                
            
            if(server->buffers[i].rsp)
                free(server->buffers[i].rsp);
        }
        free(server);
        
        TRACE(INFO,"Destroy Complete");
    }
}
