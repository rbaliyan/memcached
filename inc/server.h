#ifndef _SERVER_H_
#define _SERVER_H_

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <pthread.h>

#define RECV_TIMEOUT_MS 100
enum
{
    SERVER_STATE_NULL,
    SERVER_STATE_INIT,
    SERVER_STATE_RUNNING,
    SERVER_STATE_STOPPED,
};
enum
{
    BUFFER_STATE_FREE,
    BUFFER_STATE_USED,
    BUFFER_STATE_LOCKED,
    BUFFER_STATE_PROCESS,
};

typedef struct request_s
{
    int index;
    int state;
    int closed;
    struct sockaddr_in addr;    /* Client Remote Address */
    int sock;                   /* Socket used to send data */
    int req_size;
    int rsp_size;
    int req_len;
    int rsp_len;
    uint8_t *req;               /* Buffer */
    uint8_t *rsp;
} buffer_t;

typedef struct server_s
{
    int state;
    int free_count;
    int used_count;
    pthread_cond_t freed;
    pthread_cond_t available;
    struct sockaddr_in addr;
    int sock;
    int udp;
    int max_req_size;
    int max_rsp_size;
    pthread_t tid;
    uint32_t max_conn;
    pthread_mutex_t lock;
    buffer_t buffers[0];
} server_t;


int server_set_buffer_size(server_t *server, int req_size, int rsp_size);

server_t* server_init(short port, uint32_t  max_conn, int udp);
int server_start(server_t* server, int backlog);
int server_shutdown(server_t *server);
void server_destroy(server_t *server);
buffer_t* server_buffer_get(server_t *server);
int server_buffer_read_bytes(server_t *server, buffer_t *buffer, uint32_t size);
int server_buffer_send(server_t *server, buffer_t* buffer);
void server_buffer_release(server_t *server, buffer_t* buffer);
#endif
