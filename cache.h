/*
 * cache.h -- Cache header file for the 15-213 proxy lab
 */

/*
 * Documentation of cache structure:
 * This file contains the cache library for proxy.c.
 * I used LRU policy to handle all the evictions of cache.
 * The basic idea it that I added an order parameter in cache_block struct.
 * When read and write, update oders for all the cache block.
 * When facing evicting, move the cache block who has the maximum order.
 * I used a linked list to store all the cache blocks,
 * because it's space efficient, it won't waste a lot space.
 * All the cache_block has a next pointer to connect next block.
 * Cache_block also contains key(uri), value(response data)
 * and the size of value.
 * The head node is an empty node, which can help me find the head
 * of the list easily.
 */

#define CACHE_H

#define MAX_CACHE_SIZE (1024 * 1024)
#define MAX_OBJECT_SIZE (100 * 1024)

/* The data structure of cache block */
typedef struct block {
    char *key;
    char *value;
    int order;
    int size;
    struct block *next;
} cache_block;

/* cache interfaces */
cache_block *init_cache(cache_block *head);
void update_order(cache_block *head);
int delete_cache(cache_block *head);
int insert_cache(char *key, char *value, int len, cache_block *head,
                 int left_size);
int read_cache(char *key, int connfd, cache_block *head);
void free_cache(cache_block *head);