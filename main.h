#ifndef MAIN_H
#define MAIN_H

#include <pthread.h>
#include <stdatomic.h>
#include <signal.h>
#include <net/if.h>

#include "avl.h"

#define DEFAULT_IFACE "eth0"
#define SOCKET_PATH   "/tmp/ipstat.sock"
#define STATS_FILE    "/var/lib/ipstat/stats.txt"

extern avl_node *root;
extern pthread_mutex_t tree_lock;
extern volatile sig_atomic_t running;
extern atomic_bool capturing;
extern char current_iface[IFNAMSIZ];

void add_ip(const char *iface, const char *ip);
void save_stats();
void load_stats();
void sigint_handler(int sig);

#endif
