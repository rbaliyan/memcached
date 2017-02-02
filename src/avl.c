#include <malloc.h>
#include "avl.h"
#include "cache_data.h"

#define MODULE "AVL"
#include "trace.h"

avl_compare_t compare;
avl_free_data_t free_data;
avl_dump_data_t dump_data;

/*
 * Allocate New Node 
 *
 */ 
static avl_node_t* alloc( void * data )
{
    avl_node_t *node;

    /* Allocate New Node */
    if((node = malloc( sizeof( avl_node_t))))
    {
        node->left = node->right = NULL;
        node->height = 1;
        node->data = data;
    }
    else
    {
        TRACE(ERROR,"Failed to allocate memory");
    }
    
    return node;
}

static int max( int a, int b )
{
    return (a>b)?a:b;
}

static int height(avl_node_t *node)
{
    int h = 0;
    if( node )
        h = node->height;
        
    return h;
}

static int height_set(avl_node_t* node)
{
    int h = 0;
    if( node )
        node->height = max( height(node->left), height(node->right)) + 1;
    return h;
}

static avl_node_t* rotate_left( avl_node_t* node )
{
    avl_node_t* n1 = node->right;

    /* Rotation */
    node->right = n1->left;
    n1->left = node;

    /* Update Heights */
    height_set(node);
    height_set(n1);

    /* New Root */
    return n1;
}


static avl_node_t* rotate_right( avl_node_t* node )
{
    avl_node_t* n1 = node->left;

    /* Rotation */
    node->left = n1->right;
    n1->right = node;
    
    /* Update Heights */
    height_set(node);
    height_set(n1);

    /* New Root */
    return n1;
}

static int balance_factor( avl_node_t* node )
{
    int f = 0;
    if( node )
        f = height(node->left) - height(node->right);

    return f;
}


static int avl_compare(void* a, void* b)
{
    return compare(a,b);
}

static avl_node_t* balance( avl_node_t* node )
{
    int factor = 0;
    if( node )
    {
        height_set(node);

        factor = balance_factor( node );

        /* Balance Unbalanced */
        if( factor == 2 )
        {
            if( height(node->left->right) > height(node->left->left) )
                node->left = rotate_left(node->left);
            node = rotate_right(node);
        }
        else if ( factor ==-2 )
        {
            if ( height(node->right->left) > height(node->right->right))
                node->right = rotate_right(node->right);
            node = rotate_left(node);
        }
    }    

    return node;
}

static avl_node_t* insert( avl_node_t* head, cache_data_t *data , avl_node_t **new_node, int *status)
{
    int dt = 0;
    if( head )
    {
        /* Compare and insert */
        if(( dt = avl_compare(data, head->data)) > 0)
        {
            head->right = insert( head->right, data, new_node, status );
        }
        else if( dt < 0 )
        {
            head->left = insert( head->left, data, new_node, status );
        }
        else
        {
            /* Return dup */
            if(new_node)
                *new_node = head;
            /* Duplicate requested */
            if(*status)
                *status = 1;

            TRACE(WARN,"Duplicate Node");
        }

        /* Duplicate Entries are not created */

        /* Balance Tree */
        head = balance( head );
    }
    else
    {
        /* Create a new node */
        if((head = alloc( data )) == NULL )
        {
            /* Memeory allocation error */
            if(*status)
                *status = -1;
                
            TRACE(ERROR,"Failed to allocate memory");
        }
        else
        {
            *new_node = head;
            if(status)
                *status = 0;
        }
    }

    return head;
}


static void free_tree(avl_node_t* node)
{
    if( node )
    {
        free_tree(node->left);
        free_tree(node->right);

        /* Free Single Node */
        free_data(node->data);
        free(node);
    }
}


static void inorder(avl_node_t* node)
{
    if( node )
    {
        inorder(node->left);
        dump_data(node->data);
        inorder(node->right);
    }
}

static void preorder(avl_node_t* node)
{
    if( node )
    {
        dump_data(node->data);
        inorder(node->left);
        inorder(node->right);
    }
}

static avl_node_t* find(avl_node_t* node, void *data)
{
    int dt;
    if( node )
    {
        if(( dt = avl_compare(data, node->data)) == 0 )
            return node;
        else if( dt > 0 )
            return find(node->right, data);
        else if( dt < 0 )
            return find(node->left, data);
    }

    return NULL;
}

avl_node_t* avl_find(avl_tree_t* tree, void *data)
{
    avl_node_t* node = NULL;
  
    if( tree && data)
    {
        node = find( tree->head, data );
    }
    else
    {
        TRACE(ERROR,"Invalid args");
    }

    return node;
}

int avl_insert(avl_tree_t* tree, void *data , avl_node_t** node)
{
    int ret = -1;
    avl_node_t *new_node = NULL;
    
    if( data )
    {
        if((tree->head = insert( tree->head, data, &new_node, &ret )))
        {
            if( ret != -1 )
            {
                 if(node)
                *node = new_node;

                if(ret == 0)
                    tree->count++;
            }
            else
            {
                TRACE(ERROR,"Failed to allocate memory");
            }
        }
    }
    else
    {
        TRACE(ERROR,"Invalid args");
    }

    /*  0  : Success
     *  1  : Duplicate
     * -1  : Memory Error
     */ 
    return ret;
}



int avl_init( avl_compare_t _compare, avl_free_data_t _free_data, avl_dump_data_t _dump_data )
{
    int ret = -1;
    if(( compare == NULL  ) && ( free_data == NULL )
     && (_compare && _free_data ))
    {
        /* Init tree */
        free_data =  _free_data;
        compare = _compare;
        dump_data = _dump_data;
        ret = 0;
    }
    else
    {
        TRACE(ERROR,"Invalid args");
    }
    
    return ret;    
}

avl_tree_t* avl_create( void )
{
    avl_tree_t * tree = malloc( sizeof(avl_tree_t));
    if( tree )
    {
        tree->count = 0;
        tree->head = NULL;
    }
    else
    {
        TRACE(ERROR,"Failed to allocate memory");
    }

    return tree;
}

void avl_destroy( avl_tree_t *tree )
{
    if( tree )
    {
        /*TRACE(INFO,"Destroy");*/
        free_tree(tree->head);
        free(tree);
    }
}

void avl_inorder(avl_tree_t *tree)
{
    if( tree )
    {
        inorder(tree->head);
    }
}

void avl_preorder(avl_tree_t *tree)
{
    if( tree )
    {
        preorder(tree->head);
    }
}
