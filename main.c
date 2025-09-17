#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <pthread.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <linux/if_packet.h>
#include <net/if.h>
#include <netinet/ip.h>
#include <sys/un.h>
#include <linux/if_ether.h>
#include <sys/stat.h>

#define DEFAULT_IFACE "eth0" // standard interface for capturing
#define SOCKET_PATH "/home/vboxuser/.ipstat.sock" // path to UNIX socket
#define STATS_FILE "/home/vboxuser/stats.txt" // file where statistics are stored
#define KEY_MAX 128

// AVL-tree and its balance
typedef struct avl_node {
    char key[KEY_MAX];
    char iface[64];
    char ip[64];
    unsigned long count;
    int height;
    struct avl_node *left, *right;
} avl_node;

static avl_node *root = NULL;
static pthread_mutex_t tree_lock = PTHREAD_MUTEX_INITIALIZER;
static volatile sig_atomic_t running = 1;
static volatile sig_atomic_t capturing = 0;
static char current_iface[IFNAMSIZ] = DEFAULT_IFACE;

static int maxi(int a,int b){ return a>b?a:b; }
static int height(avl_node *n){ return n?n->height:0; }

static avl_node* new_node(const char *iface,const char *ip,unsigned long cnt){
    avl_node *n = calloc(1,sizeof(avl_node));
    snprintf(n->key, KEY_MAX, "%s|%s", iface, ip);
    strncpy(n->iface, iface, sizeof(n->iface)-1);
    strncpy(n->ip, ip, sizeof(n->ip)-1);
    n->count = cnt;
    n->height = 1;
    return n;
}

static avl_node* right_rotate(avl_node *y){
    avl_node *x = y->left;
    avl_node *T2 = x->right;
    x->right = y;
    y->left = T2;
    y->height = maxi(height(y->left),height(y->right))+1;
    x->height = maxi(height(x->left),height(x->right))+1;
    return x;
}

static avl_node* left_rotate(avl_node *x){
    avl_node *y = x->right;
    avl_node *T2 = y->left;
    y->left = x;
    x->right = T2;
    x->height = maxi(height(x->left),height(x->right))+1;
    y->height = maxi(height(y->left),height(y->right))+1;
    return y;
}

static int get_balance(avl_node *n){ return n? height(n->left)-height(n->right):0; }

static avl_node* avl_insert(avl_node* node,const char *iface,const char *ip,unsigned long cnt){
    char key[KEY_MAX]; snprintf(key, KEY_MAX, "%s|%s", iface, ip);
    if(!node) return new_node(iface, ip, cnt);

    int cmp = strcmp(key,node->key);
    if(cmp==0){ node->count+=cnt; return node; }
    else if(cmp<0) node->left = avl_insert(node->left, iface, ip, cnt);
    else node->right = avl_insert(node->right, iface, ip, cnt);

    node->height = 1 + maxi(height(node->left), height(node->right));
    int balance = get_balance(node);

    if(balance>1 && strcmp(key,node->left->key)<0) return right_rotate(node);
    if(balance<-1 && strcmp(key,node->right->key)>0) return left_rotate(node);
    if(balance>1 && strcmp(key,node->left->key)>0){ node->left=left_rotate(node->left); return right_rotate(node);}
    if(balance<-1 && strcmp(key,node->right->key)<0){ node->right=right_rotate(node->right); return left_rotate(node);}
    return node;
}

static unsigned long avl_count_by_ip(avl_node* node,const char *ip){
    if(!node) return 0;
    unsigned long s=0;
    s+=avl_count_by_ip(node->left,ip);
    if(strcmp(node->ip,ip)==0) s+=node->count;
    s+=avl_count_by_ip(node->right,ip);
    return s;
}

static void avl_print_stat(avl_node* node,const char *iface, FILE *out){
    if(!node) return;
    avl_print_stat(node->left, iface, out);
    if(!iface || strcmp(node->iface,iface)==0) fprintf(out,"%s %s %lu\n",node->iface,node->ip,node->count);
    avl_print_stat(node->right, iface, out);
}

static void free_tree(avl_node *n){ if(!n) return; free_tree(n->left); free_tree(n->right); free(n); }

static void add_ip(const char *iface,const char *ip){
    pthread_mutex_lock(&tree_lock);
    root = avl_insert(root, iface, ip, 1);
    pthread_mutex_unlock(&tree_lock);
}

// save/load stats
static void save_stats(){
    FILE *f=fopen(STATS_FILE,"w");
    if(!f) return;
    pthread_mutex_lock(&tree_lock);
    avl_print_stat(root,NULL,f);
    pthread_mutex_unlock(&tree_lock);
    fclose(f);
}

static void load_stats(){
    FILE *f=fopen(STATS_FILE,"r");
    if(!f) return;
    char ifn[64],ip[64]; unsigned long cnt;
    while(fscanf(f,"%63s %63s %lu",ifn,ip,&cnt)==3){
        root = avl_insert(root,ifn,ip,cnt);
    }
    fclose(f);
}

// packet handler
void *capture_thread(void *arg){
    (void)arg;
    unsigned char *buf = malloc(65536);
    if(!buf) return NULL;

    int sock_raw = socket(AF_PACKET, SOCK_RAW, htons(ETH_P_IP));
    if(sock_raw<0){ perror("socket"); return NULL; }

    while(running){
        if(!capturing){ sleep(1); continue; }

        pthread_mutex_lock(&tree_lock);
        char iface[IFNAMSIZ]; strncpy(iface,current_iface,IFNAMSIZ-1); iface[IFNAMSIZ-1]=0;
        pthread_mutex_unlock(&tree_lock);

        struct sockaddr_ll sll={0};
        sll.sll_family = AF_PACKET;
        sll.sll_ifindex = if_nametoindex(iface);
        sll.sll_protocol = htons(ETH_P_IP);
        if(bind(sock_raw,(struct sockaddr*)&sll,sizeof(sll))<0){ sleep(1); continue; }

        struct sockaddr saddr; socklen_t saddr_len=sizeof(saddr);
        int size=recvfrom(sock_raw,buf,65536,0,&saddr,&saddr_len);
        if(size<(int)sizeof(struct iphdr)) continue;
        struct iphdr *iph=(struct iphdr*)buf;
        struct in_addr src; src.s_addr=iph->saddr;
        add_ip(iface,inet_ntoa(src));
        save_stats();
    }
    close(sock_raw);
    free(buf);
    return NULL;
}

// client handler
void handle_client(int fd){
    char buf[256]; ssize_t r=read(fd,buf,sizeof(buf)-1);
    if(r<=0){ close(fd); return; }
    buf[r]=0; char *nl=strchr(buf,'\n'); if(nl)*nl=0;

    if(strcmp(buf,"start")==0){ capturing=1; dprintf(fd,"OK: capturing started on %s\n",current_iface);}
    else if(strcmp(buf,"stop")==0){ capturing=0; dprintf(fd,"OK: capturing stopped\n");}
    else if(strncmp(buf,"select iface ",13)==0){
        pthread_mutex_lock(&tree_lock);
        strncpy(current_iface,buf+13,IFNAMSIZ-1); current_iface[IFNAMSIZ-1]=0;
        pthread_mutex_unlock(&tree_lock);
        dprintf(fd,"OK: selected iface %s\n",current_iface);
    }
    else if(strncmp(buf,"show ",5)==0){
        char ip[64],tail[64]; if(sscanf(buf+5,"%63s %63s",ip,tail)==2 && strcmp(tail,"count")==0){
            unsigned long cnt=avl_count_by_ip(root,ip);
            dprintf(fd,"%s count: %lu\n",ip,cnt);
        } else dprintf(fd,"ERR: usage show <ip> count\n");
    }
    else if(strncmp(buf,"stat",4)==0){
        char *arg = (strlen(buf)>5)? buf+5 : NULL;
        FILE *tmpf=tmpfile();
        pthread_mutex_lock(&tree_lock);
        avl_print_stat(root,arg,tmpf);
        pthread_mutex_unlock(&tree_lock);
        fflush(tmpf); fseek(tmpf,0,SEEK_SET);
        char line[256]; while(fgets(line,sizeof(line),tmpf)) write(fd,line,strlen(line));
        fclose(tmpf);
    }
    else if(strcmp(buf,"--help")==0){ dprintf(fd,"Commands: \n start \n, stop \n, select iface <name> \n, show <ip> count \n, stat [iface] \n, --help \n"); }
    else dprintf(fd,"ERR: unknown command\n");
    close(fd);
}

void *client_thread(void *arg){ int fd=*(int*)arg; free(arg); handle_client(fd); return NULL; }

// creates a UNIX socket
void *server_thread(void *arg){
    (void)arg;
    int sfd=socket(AF_UNIX,SOCK_STREAM,0); if(sfd<0) return NULL;
    struct sockaddr_un addr; memset(&addr,0,sizeof(addr));
    addr.sun_family=AF_UNIX; strncpy(addr.sun_path,SOCKET_PATH,sizeof(addr.sun_path)-1);
    unlink(SOCKET_PATH); bind(sfd,(struct sockaddr*)&addr,sizeof(addr));
    chmod(SOCKET_PATH,0660);
    listen(sfd,5);
    while(running){
        int *cfd=malloc(sizeof(int));
        *cfd=accept(sfd,NULL,NULL);
        if(*cfd<0){ free(cfd); continue; }
        pthread_t t; pthread_create(&t,NULL,client_thread,cfd); pthread_detach(t);
    }
    close(sfd); unlink(SOCKET_PATH);
    return NULL;
}
// signal handler
void sigint_handler(int sig){ (void)sig; running=0; }

int main(){
    signal(SIGINT,sigint_handler);
    signal(SIGTERM,sigint_handler);

    load_stats();

    pthread_t srv,cap;
    pthread_create(&srv,NULL,server_thread,NULL);
    pthread_create(&cap,NULL,capture_thread,NULL);

    while(running) sleep(1);

    capturing=0;
    pthread_join(cap,NULL);
    save_stats();
    free_tree(root);
    return 0;
}
