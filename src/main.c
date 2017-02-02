#include <stdio.h>
#include <signal.h>
#include <unistd.h>
#include <stdlib.h>
#include "memcached.h"
#include "server.h"

#define MODULE "Main"
#include "trace.h"

#define MAX_VER_STR 16
#define NEXT_ARGV(x)  (((x + 1) < argc) ? argv[i+1] : NULL )

static int ver_major = 0;
static int ver_minor = 1;
static int ver_patch = 1;

static memcached_t *mc;
static server_t *server;

static int port = 5000;
static int hash_size = 256;
static int tcount = 1;
static int max_key = 0;
static int max_val = 0;
static int verbose = 2;
static int udp = 0;
char *app_name = NULL;


static void cleanup(void);
static void singnal_handler(int signo);
static void set_cleanup( void );
static void usage( void );
static void invalid_args(const char *msg);
static void parse_args(int argc, char *argv[]);
static const char* get_version( void );

/*
 * Returns Current Version
 */ 
const static char* get_version( void )
{
    static char version_str[MAX_VER_STR];
    
    snprintf(version_str,MAX_VER_STR,"%d.%d.%d",ver_major,ver_minor,ver_patch);
    
    return version_str;
}


/* 
 * Perform System Cleanup 
 */ 
static void cleanup(void)
{
    TRACE(INFO,"Start Cleanup");
    /* Destory Instance */
    memcached_destroy(mc);
    server = NULL;
    mc = NULL;
    TRACE(INFO,"Cleanup Done");
    
}

/*
 *  Signal Handler
 */ 
static void singnal_handler(int signo)
{
    if(( signo == SIGINT )|| ( signo == SIGTERM ))
    {
        TRACE(INFO,"Signal Recevied");
        cleanup();
        
        exit(0);
    }
}

/*
 * Set System Cleanup
 */ 
static void set_cleanup( void )
{
    if (signal(SIGINT, singnal_handler) == SIG_ERR)
    {
        TRACE(ERROR,"Failed to insall singnal");
    }
    
    
    if (signal(SIGTERM, singnal_handler) == SIG_ERR)
    {
        TRACE(ERROR,"Failed to insall singnal");
    }
}

/* 
 * Print Application usage 
 */
static void usage( void )
{
    printf("usage : %s [options]\n", app_name);
    printf("-h      : help\n");
    printf("-V      : Print Version\n");
    printf("-v      : verbose\n");
    printf("-u      : udp connection, default, 0\n");
    printf("-p port : port, default %d\n", port);
    printf("-k key_len : Max Key Len, default, %d\n", MCACHE_KEY_LEN_DEFAULT );
    printf("-l val_len : Max Value Len, default, %d\n", MCACHE_KEY_LEN_DEFAULT );
    printf("-t thread count : Parallel Threads, default, %d\n", tcount);
    printf("-H Hash size : Hash Size, default, %d\n", hash_size);
}

/* 
 * Handle Invalid Arguments
 */ 
void invalid_args(const char *msg)
{
    printf("%sn",msg);
    usage();
    exit(1);
}

/*
 * Parse Integer Argument
 */
int parse_int(char* str, char *next,char *msg, int *val)
{
    char *ptr = NULL;
    int ret = 0;

    if(str[1])
    {
        ptr = &str[1];
    }
    else
    {
        ptr = next;
        ret = 1;
    }
    
    if(ptr == NULL || (sscanf(ptr, "%d", val) != 1))
    {
        invalid_args(msg);
    }
    
    return ret;
}


/*
 * Parse Command line arguments 
 */ 
static void parse_args(int argc, char *argv[])
{
    int i = 0;
    int j=0;
    char *str;
    int veb  = 0;
    
    /* Set App Name */
    app_name = argv[0];
    
    for(i=1;i<argc;i++)
    {
        str = argv[i];
        if((str[0] == '-'))
        {
            switch(str[1])
            {
                case 'h':
                    usage();
                    exit(0);
                break;
                case 'u':
                    udp=1;
                break;
                case 'V':
                    printf("%s Current Version : %s\n",app_name, get_version());
                    exit(0);
                break;
                    
                case 'v':
                    for( j = 1;str[j] && str[j] == 'v';j++);
                    
                    veb +=(j-1);
                    verbose = set_trace_level(veb);
                    
                break;
                case 'p':
                    i+=parse_int(&str[1], NEXT_ARGV(i), "invalid port\n",&port );                   
                break;
                case 'k':
                    i+=parse_int(&str[1], NEXT_ARGV(i), "invalid key len\n",&max_key);
                break;
                case 'l':
                    i+=parse_int(&str[1], NEXT_ARGV(i), "invalid value len\n",&max_val );                    
                break;
                case 't':
                    i+=parse_int(&str[1], NEXT_ARGV(i), "invalid hread cot\n",&tcount );                   
                break;
                case 'H':
                    i+=parse_int(&str[1], NEXT_ARGV(i), "invalid port\n",&hash_size );                   
                break;
                default:
                        usage();
                        exit(0);
                break;     
            }
        }
        else
        {
            printf("invalid option %s", argv[i]);
            usage();
            exit(0);
        }
    }
}

/*
 * Main Function 
 */ 
int main( int argc, char *argv[])
{   
    /* Parse Argument */
    parse_args(argc,argv);
    
    verbose = set_trace_level(verbose);
    
    TRACE(INFO,"Conection : %s",( udp ? "udp" : "tcp"));
    TRACE(INFO,"Port : %d", port);
    TRACE(INFO,"Hash Size : %d", hash_size);
    TRACE(INFO,"Thead Count : %d",tcount);
    
    /* Enable Cleanup */
    set_cleanup();
    
    TRACE(DEBUG,"Init Server : %s");
    server = server_init(port,tcount,udp);

    TRACE(DEBUG,"Memcached init");
    mc = memcached_init(server,tcount,hash_size);

    TRACE(DEBUG,"Start Memcached");
    if(memcached_start(mc))
    {
        TRACE(ERROR,"Start Memcached Failed");
        memcached_destroy(mc);
        mc = NULL;
    }

    while(mc)
    {
       /* sleep main thread */
       usleep(100);
    }


    return 0;
}

