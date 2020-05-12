/*
 * cache.c -- Cache library for the 15-213 proxy lab
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

#include "cache.h"
#include "csapp.h"

#include <assert.h>
#include <ctype.h>
#include <inttypes.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>

#include <errno.h>
#include <netdb.h>
#include <netinet/in.h>
#include <pthread.h>
#include <signal.h>
#include <sys/socket.h>
#include <sys/types.h>

static pthread_mutex_t mutex;

/*
 * init_cache - init ten cache head.
 */
cache_block *init_cache(cache_block *head) {
    head = malloc(sizeof(cache_block));
    head->next = NULL;
    pthread_mutex_init(&mutex, NULL);
    return head;
}

/*
 * update_order - update the order for all the cache block in the cache list.
 */
void update_order(cache_block *head) {
    cache_block *tmp = head->next;
    while (tmp != NULL) {
        if (tmp->order != 0) {
            tmp->order += 1;
        }
        tmp = tmp->next;
    }
}

/*
 * update_order - delete the cache block who has the maximum order number.
 * Return this block's size.
 */
int delete_cache(cache_block *head) {
    cache_block *tmp = head->next;
    cache_block *node;
    int max = 0;
    int size = 0;
    // find the maximum order block
    while (tmp != NULL) {
        if (tmp->order > max) {
            max = tmp->order;
        }
        tmp = tmp->next;
    }
    tmp = head;
    // delete block.
    while (tmp->next != NULL) {
        if (tmp->next->order == max) {
            node = tmp->next;
            tmp->next = tmp->next->next;
            size = node->size;
            free(node->key);
            free(node->value);
            free(node);
            break;
        }
        tmp = tmp->next;
    }
    return size;
}

/*
 * insert_cache - create a block with key-value pair, if cace list
 * does not contains this key(uri), insert this block into the
 * head of the cache list and update orders for cache list.
 * Else update orders for cache list and return.
 * Return left_size after inserting.
 */
int insert_cache(char *key, char *value, int len, cache_block *head,
                 int left_size) {
    cache_block *node = head->next;
    while (node != NULL) {
        if (strcmp(node->key, key) == 0) {
            node->order = 1;
            update_order(head);
            return left_size;
        }
        node = node->next;
    }
    cache_block *tmp;
    while (left_size < len) {
        left_size += delete_cache(head);
    }
    tmp = malloc(sizeof(cache_block));
    tmp->key = malloc(sizeof(char) * MAXLINE);
    tmp->value = malloc(sizeof(char) * len);
    tmp->order = 1;
    tmp->size = len;
    tmp->next = head->next;
    head->next = tmp;
    left_size -= len;
    memcpy(tmp->key, key, MAXLINE);
    memcpy(tmp->value, value, len);
    update_order(head);
    return left_size;
}

/*
 * read_cache - read the cache list, find if any block's key match with key.
 * Write the block's value to connfd if match, return 1, else return -1.
 */
int read_cache(char *key, int connfd, cache_block *head) {
    cache_block *tmp = head->next;
    int res = -1;
    while (tmp != NULL) {
        if (strcmp(tmp->key, key) == 0) {
            tmp->order = 1;
            update_order(head);
            pthread_mutex_unlock(&mutex);
            rio_writen(connfd, tmp->value, tmp->size);
            res = 1;
            return res;
        }
        tmp = tmp->next;
    }
    update_order(head);
    return res;
}

/*
 * free_cache - free all the cache blocks.
 */
void free_cache(cache_block *head) {
    cache_block *tmp = head;
    cache_block *node;
    while (tmp != NULL) {
        node = tmp->next;
        free(tmp->key);
        free(tmp->value);
        free(tmp);
        tmp = node;
    }
    free(head);
}