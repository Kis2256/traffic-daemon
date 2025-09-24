#include "avl.h"
#include <stdlib.h>
#include <string.h>

static int maxi(int a, int b)
{
    if (a > b) {
        return a;
    } else {
        return b;
    }
}
static int height(avl_node *n)
{
    if (n != NULL) {
        return n->height;
    } else {
        return 0;
    }
}

static avl_node* new_node(const char *iface, const char *ip, unsigned long cnt) {
    avl_node *n = calloc(1,sizeof(avl_node));
    if (!n)
    {
        return NULL;
    }

    snprintf(n->key, KEY_MAX, "%s|%s", iface, ip);
    strncpy(n->iface, iface, sizeof(n->iface)-1);
    strncpy(n->ip, ip, sizeof(n->ip)-1);
    n->count = cnt;
    n->height = 1;
    return n;
}

static avl_node* right_rotate(avl_node *y) {
    avl_node *x = y->left;
    avl_node *T2 = x->right;

    x->right = y;
    y->left = T2;

    y->height = maxi(height(y->left), height(y->right))+1;
    x->height = maxi(height(x->left), height(x->right))+1;

    return x;
}

static avl_node* left_rotate(avl_node *x) {
    avl_node *y = x->right;
    avl_node *T2 = y->left;

    y->left = x;
    x->right = T2;

    x->height = maxi(height(x->left), height(x->right))+1;
    y->height = maxi(height(y->left), height(y->right))+1;

    return y;
}

static int get_balance(avl_node *n) {
    if (n != NULL) {
        return height(n->left) - height(n->right);
    } else {
        return 0;
    }
}

avl_node* avl_insert(avl_node* node, const char *iface, const char *ip, unsigned long cnt) {
    char key[KEY_MAX];
    int balance = get_balance(node);

    snprintf(key, KEY_MAX, "%s|%s", iface, ip);

    if(!node)
    {
        return new_node(iface, ip, cnt);
    }

    int cmp = strcmp(key,node->key);
    if(cmp==0) {
        node->count += cnt;
        return node;
    }
    else if(cmp < 0) {
        node->left = avl_insert(node->left, iface, ip, cnt);
    }
    else {
        node->right = avl_insert(node->right, iface, ip, cnt);
    }

    node->height = 1 + maxi(height(node->left), height(node->right));

    if(balance > 1 && strcmp(key,node->left->key) < 0)
    {
        return right_rotate(node);
    }

    if(balance < -1 && strcmp(key,node->right->key) > 0)
    {
        return left_rotate(node);
    }

    if(balance > 1 && strcmp(key,node->left->key) > 0) {
        node->left = left_rotate(node->left);
        return right_rotate(node);
    }

    if(balance < -1 && strcmp(key,node->right->key) < 0) {
        node->right = right_rotate(node->right);
        return left_rotate(node);
    }

    return node;
}

unsigned long avl_count_by_ip(avl_node* node, const char *ip) {
    if(!node)
    {
        return 0;
    }
    unsigned long s=0;
    s += avl_count_by_ip(node->left, ip);
    if(strcmp(node->ip, ip) == 0)
    {
        s += node->count;
    }
    s += avl_count_by_ip(node->right, ip);
    return s;
}

void avl_print_stat(avl_node* node,const char *iface, FILE *out) {
    if(!node)
    {
        return;
    }
    avl_print_stat(node->left, iface, out);
    if(!iface || strcmp(node->iface,iface) == 0){
        fprintf(out,"%s %s %lu\n", node->iface, node->ip, node->count);
}
    avl_print_stat(node->right, iface, out);
}

void free_tree(avl_node *n) {
    if(!n)
    {
        return;
    }
    free_tree(n->left);
    free_tree(n->right);
    free(n);
}
