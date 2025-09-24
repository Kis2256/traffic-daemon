#include "main.h"
#include "server.h"

#include <sys/un.h>
#include <sys/socket.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>

static void handle_client(int fd){
    char buf[256];
    ssize_t r=read(fd,buf,sizeof(buf)-1);
    buf[r]=0;
    char *nl=strchr(buf,'\n');
    if(r<=0)
    {
        close(fd);
        return;
    }
    if(nl)
    {
        *nl=0;
    }
    if(strcmp(buf,"start")==0){
        atomic_store(&capturing,1);
        dprintf(fd,"OK: capturing started on %s\n",current_iface);
    }
    else if(strcmp(buf,"stop")==0){
        atomic_store(&capturing,0);
        dprintf(fd,"OK: capturing stopped\n");
    }
    else if(strncmp(buf,"select iface ",13)==0){
        pthread_mutex_lock(&tree_lock);
        strncpy(current_iface,buf+13,IFNAMSIZ-1);
        current_iface[IFNAMSIZ-1]=0;
        pthread_mutex_unlock(&tree_lock);
        dprintf(fd,"OK: selected iface %s\n",current_iface);
    }
    else if(strncmp(buf,"show ",5)==0){
        char ip[64],tail[64];
        if(sscanf(buf+5,"%63s %63s",ip,tail)==2 && strcmp(tail,"count")==0){
            unsigned long cnt=avl_count_by_ip(root,ip);
            dprintf(fd,"%s count: %lu\n",ip,cnt);
        } else
        {
            dprintf(fd,"ERR: usage show <ip> count\n");
        }
    }
    else if(strncmp(buf,"stat",4)==0){
        char line[256];
        char *arg = (strlen(buf)>5)? buf+5 : NULL;
        FILE *tmpf=tmpfile();
        pthread_mutex_lock(&tree_lock);
        avl_print_stat(root,arg,tmpf);
        pthread_mutex_unlock(&tree_lock);
        fflush(tmpf);
        fseek(tmpf,0,SEEK_SET);
        while(fgets(line,sizeof(line),tmpf))
        {
            write(fd,line,strlen(line));
        }
        fclose(tmpf);
    }
    else if(strcmp(buf,"--help")==0){
        dprintf(fd,
            "Commands:\n"
            "  start\n"
            "  stop\n"
            "  select iface <name>\n"
            "  show <ip> count\n"
            "  stat [iface]\n"
            "  --help\n");
    }
    else
    {
        dprintf(fd,"ERR: unknown command\n");
    }
    close(fd);
}

static void *client_thread(void *arg){
    int fd=*(int*)arg;
    free(arg);
    handle_client(fd);
    return NULL;
}

void *server_thread(void *arg){
    (void)arg;
    int sfd=socket(AF_UNIX,SOCK_STREAM,0);
    if(sfd<0)
    {
        return NULL;
    }

    struct sockaddr_un addr;
    memset(&addr,0,sizeof(addr));
    addr.sun_family=AF_UNIX;
    strncpy(addr.sun_path,SOCKET_PATH,sizeof(addr.sun_path)-1);

    unlink(SOCKET_PATH);
    bind(sfd,(struct sockaddr*)&addr,sizeof(addr));
    chmod(SOCKET_PATH,0660);
    listen(sfd,5);

    while(running){
        int *cfd=malloc(sizeof(int));
        *cfd=accept(sfd,NULL,NULL);
        if(*cfd<0)
        {
            free(cfd);
            continue;
        }
        pthread_t t;
        pthread_create(&t,NULL,client_thread,cfd);
        pthread_detach(t);
    }
    close(sfd);
    unlink(SOCKET_PATH);
    return NULL;
}
