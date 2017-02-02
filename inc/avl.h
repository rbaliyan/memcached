#ifndef _AVL_H_
#define _AVL_H_

typedef struct avl_node_s
{
    int height;
    struct avl_node_s *left;
    struct avl_node_s* right;
    void *data;
} avl_node_t;

typedef int (*avl_compare_t)(avl_node_t* node1, avl_node_t *node2);
typedef int (*avl_free_data_t)(void* data);
typedef void (*avl_dump_data_t)(void* data);
typedef struct avl_tree_s
{
    int count;
    avl_node_t *head;
} avl_tree_t;

int avl_insert(avl_tree_t* tree, void *data , avl_node_t** dup);
avl_node_t *avl_find(avl_tree_t* tree, void *data);

int avl_init( avl_compare_t compare, avl_free_data_t free_data, avl_dump_data_t dump_data );
avl_tree_t* avl_create( void );
void avl_destroy( avl_tree_t *tree );
void avl_preorder(avl_tree_t *tree);
void avl_inorder(avl_tree_t *tree);

#endif
