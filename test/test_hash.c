#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "avl.h"
#include "cache_data.h"
#include "hash_table.h"

int main()
{
    
    hash_table_t *ht = NULL;
    hash_table_init();

    ht = hash_table_create(256);

    #define TEST_SZ 6

    uint8_t* x[TEST_SZ]={"Hello","Test1","Test2","Test3","Test4","Test5"};
    uint8_t* y[TEST_SZ]={"Hellov","Test1v","Test2v","Test3v","Test4v","Test5v"};
    cache_data_t* d[TEST_SZ] = {0};
    int i = 0;
    if( ht )
    {
        for(i=0;i<TEST_SZ;i++)
        {
            d[i] = cache_data_alloc(strlen(x[i]),strlen(y[i]),x[i],y[i]);
            printf("insert : %d\n", hash_table_insert(ht, d[i], NULL));

            //avl_inorder(tree);
            //printf("\n\n\n");
            //avl_preorder(tree);
        }

        for(i=0;i<TEST_SZ;i++)
            cache_data_dump(d[i]);
        printf("\n#######\n");

        //avl_inorder(ht->table[0].tree);
        //printf("\n\n\n");
        //avl_preorder(ht->table[0].tree);
        hash_data_node_t *h = NULL;

        for(i=0;i<TEST_SZ;i++)
            if((h = hash_table_search(ht, d[i])))
                cache_data_dump(h->data);
            else
                printf("Not Found");

        cache_data_t *d1 = cache_data_alloc(7,6,"Hello1","Value");

        if((h = hash_table_search(ht, d1)))
                cache_data_dump(h->data);
            else
                printf("Not Found");
    }
}
