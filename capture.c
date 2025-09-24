#include "main.h"
#include "capture.h"

#include <arpa/inet.h>
#include <netinet/ip.h>
#include <netinet/ether.h>
#include <linux/if_packet.h>
#include <sys/socket.h>
#include <sys/poll.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

void *capture_thread(void *arg){
    (void)arg;
    unsigned char *buf = malloc(65536);
    if(!buf)
    {
        return NULL;
    }

    int sock_raw = -1;
    struct pollfd pfd;

    while(running){
        if(!atomic_load(&capturing)){
            if(sock_raw != -1){
                close(sock_raw);
                sock_raw = -1;
            }
            usleep(200000);
            continue;
        }

        if(sock_raw == -1){
            sock_raw = socket(AF_PACKET, SOCK_RAW, htons(ETH_P_ALL));
            if(sock_raw < 0)
            {
                perror("socket"); free(buf);
                return NULL;
            }

            int flags = fcntl(sock_raw, F_GETFL, 0);
            fcntl(sock_raw, F_SETFL, flags | O_NONBLOCK);

            pthread_mutex_lock(&tree_lock);
            char iface[IFNAMSIZ];
            strncpy(iface, current_iface, IFNAMSIZ-1);
            iface[IFNAMSIZ-1] = 0;
            pthread_mutex_unlock(&tree_lock);

            struct sockaddr_ll sll = {0};
            sll.sll_family   = AF_PACKET;
            sll.sll_ifindex  = if_nametoindex(iface);
            sll.sll_protocol = htons(ETH_P_ALL);

            if (bind(sock_raw,(struct sockaddr*)&sll,sizeof(sll)) < 0) {
                perror("bind");
                close(sock_raw);
                sock_raw = -1;
                sleep(1);
                continue;
            }
            pfd.fd = sock_raw;
            pfd.events = POLLIN;
        }

        int ret = poll(&pfd, 1, 500);
        if (ret <= 0)
        {
            continue;
        }
        if (pfd.revents & POLLIN) {
            struct sockaddr saddr;
            socklen_t saddr_len = sizeof(saddr);
            int size = recvfrom(sock_raw, buf, 65536, 0, &saddr, &saddr_len);
            if(size < (int)(sizeof(struct ethhdr) + sizeof(struct iphdr)))
            {
                continue;
            }
            struct ethhdr *eth = (struct ethhdr*)buf;
            if (ntohs(eth->h_proto) != ETH_P_IP)
            {
                continue;
            }
            struct iphdr *iph = (struct iphdr*)(buf + sizeof(struct ethhdr));
            struct in_addr src;
            src.s_addr = iph->saddr;

            char ipbuf[INET_ADDRSTRLEN];
            if (inet_ntop(AF_INET, &src, ipbuf, sizeof(ipbuf)) != NULL) {
                pthread_mutex_lock(&tree_lock);
                char iface[IFNAMSIZ];
                strncpy(iface, current_iface, IFNAMSIZ-1);
                iface[IFNAMSIZ-1] = 0;
                pthread_mutex_unlock(&tree_lock);
                add_ip(iface, ipbuf);
                save_stats();
            }
        }
    }
    if(sock_raw != -1)
    {
        close(sock_raw);
    }
    free(buf);
    return NULL;
}
