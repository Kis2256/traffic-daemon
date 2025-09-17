#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/un.h>
#include <sys/socket.h>
#include <unistd.h>
#include <sys/wait.h>

#define SOCKET_PATH "/home/vboxuser/.ipstat.sock"
#define DAEMON_PATH "./daemon"

int connect_to_daemon() {
    int sfd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (sfd < 0) { perror("socket"); return -1; }

    struct sockaddr_un addr;
    memset(&addr,0,sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, SOCKET_PATH, sizeof(addr.sun_path)-1);

    if (connect(sfd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        close(sfd);
        return -1;
    }
    return sfd;
}

void start_daemon() {
    pid_t pid = fork();
    if (pid < 0) { perror("fork"); exit(1); }
    if (pid == 0) {
        if (setsid() < 0) exit(1);
        execl(DAEMON_PATH, DAEMON_PATH, NULL);
        perror("execl"); exit(1);
    }
    sleep(1);
}

int main(int argc,char **argv){
    if(argc<2){ printf("Usage: cli <command>\n"); return 0; }

    int sfd = connect_to_daemon();
    if (sfd < 0) {
        printf("Daemon not running, starting it...\n");
        start_daemon();
        sfd = connect_to_daemon();
        if (sfd < 0) { perror("connect"); return 1; }
    }

    char buf[256];
    snprintf(buf,sizeof(buf),"%s",argv[1]);
    for(int i=2;i<argc;i++){ strcat(buf," "); strcat(buf,argv[i]); }
    strcat(buf,"\n");
    write(sfd,buf,strlen(buf));

    while(1){
        int r=read(sfd,buf,sizeof(buf)-1);
        if(r<=0) break;
        buf[r]=0;
        printf("%s",buf);
    }
    close(sfd);
    return 0;
}
