#include "main.h"
#include "capture.h"
#include "server.h"
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "avl.h"

avl_node *root = NULL;
pthread_mutex_t tree_lock = PTHREAD_MUTEX_INITIALIZER;
volatile sig_atomic_t running = 1;
atomic_bool capturing = 0;
char current_iface[IFNAMSIZ] = DEFAULT_IFACE;

void add_ip(const char *iface,const char *ip){
    pthread_mutex_lock(&tree_lock);
    root = avl_insert(root, iface, ip, 1);
    pthread_mutex_unlock(&tree_lock);
}

void save_stats(){
    FILE *f=fopen(STATS_FILE,"w");
    if(!f)
    {
        return;
    }
    pthread_mutex_lock(&tree_lock);
    avl_print_stat(root,NULL,f);
    pthread_mutex_unlock(&tree_lock);
    fclose(f);
}

void load_stats(){
    char ifn[64],ip[64];
    unsigned long cnt;
    FILE *f=fopen(STATS_FILE,"r");
    if(!f)
    {
        return;
    }
    while(fscanf(f,"%63s %63s %lu",ifn,ip,&cnt)==3){
        root = avl_insert(root,ifn,ip,cnt);
    }
    fclose(f);
}

void sigint_handler(int sig)
{
    (void)sig;
    running=0;
}

int main(){
    signal(SIGINT,sigint_handler);
    signal(SIGTERM,sigint_handler);

    load_stats();

    pthread_t srv,cap;
    pthread_create(&srv,NULL,server_thread,NULL);
    pthread_create(&cap,NULL,capture_thread,NULL);

    while(running)
    {
        sleep(1);
    }

    atomic_store(&capturing,0);
    pthread_join(cap,NULL);
    save_stats();
    free_tree(root);
    return 0;
}
