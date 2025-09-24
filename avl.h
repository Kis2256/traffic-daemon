#ifndef AVL_H
#define AVL_H

#include <stdio.h>

#define KEY_MAX 128

typedef struct avl_node {
    char key[KEY_MAX];
    char iface[64];
    char ip[64];
    unsigned long count;
    int height;
    struct avl_node *left, *right;
} avl_node;

avl_node* avl_insert(avl_node* node, const char *iface, const char *ip, unsigned long cnt);
unsigned long avl_count_by_ip(avl_node* node, const char *ip);
void avl_print_stat(avl_node* node, const char *iface, FILE *out);
void free_tree(avl_node *n);

#endif
