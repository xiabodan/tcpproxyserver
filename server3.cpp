/* File Name: server.c */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/epoll.h>
#include <iostream>
#include <fcntl.h>
#include <signal.h>

#define DEFAULT_PORT 10121
#define MAXLINE 4096
#define MAXFDS 256
#define EVENTS 100

void *thread_client(void *);
int client(void);

void *thread_client(void *junk) {
    printf("thread_client");
}

bool setNonBlock(int fd) {
    int flags = fcntl(fd, F_GETFL, 0);
    flags |= O_NONBLOCK;
    if(-1 == fcntl(fd, F_SETFL, flags))
        return false;
    return true;
}

int main(int argc, char** argv) {
    // start_client();

    int    socket_fd, connect_fd;
    struct sockaddr_in servaddr, caddr;
    char    buff[4096];
    int     n;
    //初始化Socket
    if ((socket_fd = socket(AF_INET, SOCK_STREAM, 0)) == -1 ) {
        printf("create socket error: %s(errno: %d)\n",strerror(errno),errno);
        exit(0);
    }
    /* Enable address reuse */
    int on = 1;
    setsockopt(socket_fd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on));
    //初始化
    memset(&servaddr, 0, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = htonl(INADDR_ANY);//IP地址设置成INADDR_ANY,让系统自动获取本机的IP地址。
    servaddr.sin_port = htons(DEFAULT_PORT);//设置的端口为DEFAULT_PORT

    //将本地地址绑定到所创建的套接字上
    if (bind(socket_fd, (struct sockaddr*)&servaddr, sizeof(servaddr)) == -1) {
        printf("bind socket error: %s(errno: %d)\n",strerror(errno),errno);
        exit(0);
    }
    //开始监听是否有客户端连接
    if (listen(socket_fd, 10) == -1){
        printf("listen socket error: %s(errno: %d)\n",strerror(errno),errno);
        exit(0);
    }
    printf("listen socket_fd %d\n", socket_fd);
    // 创建epoll
    int nfds;
    int cfd;
    char buffer[512];
    struct epoll_event ev, events[EVENTS];
    int epfd = epoll_create(MAXFDS);
    ev.data.fd = socket_fd;
    ev.events = EPOLLIN | EPOLLET;
    epoll_ctl(epfd, EPOLL_CTL_ADD, socket_fd, &ev);

    printf("======waiting for client's request======\n");
    while (1) {
        nfds = epoll_wait(epfd, events, MAXFDS, 2000);
        // printf("epoll_wait respond nfds %d\n", nfds);
        for (int i = 0; i < nfds; ++i) {
            if (socket_fd == events[i].data.fd) {
                memset(&caddr, 0, sizeof(caddr) );
                int len = sizeof(caddr);
                cfd = accept(socket_fd, (struct sockaddr *)&caddr, &len);
                if (-1 == cfd) {
                    printf("服务器接收套接字的时候出问题了");
                    break;
                }
                setNonBlock(cfd);
                printf("accept cfg %d\n", cfd);

                struct epoll_event ev_con;
                ev_con.data.fd = cfd;
                ev_con.events = EPOLLIN | EPOLLET;
                epoll_ctl(epfd, EPOLL_CTL_ADD, cfd, &ev_con);
            } else if (events[i].data.fd & EPOLLIN) {
                bzero(buffer, sizeof(buffer));
                printf("服务器端要读取客户端发过来的消息 fd %d\n", events[i].data.fd);
                int len = recv(events[i].data.fd, buffer, sizeof(buffer), 0);
                if (len == 0) {
                    printf("connect disconnected %d\n", events[i].data.fd);
                    ev.data.fd = events[i].data.fd;
                    epoll_ctl(epfd, EPOLL_CTL_DEL, events[i].data.fd, &ev);
                    close(events[i].data.fd);
                    continue;
                } else if (len < 0) {
                    printf("connect_fd recv error: %s(errno: %d)\n", strerror(errno), errno);
                } else {
                    printf("cfd %d 接收到proxy消息: %s\n", events[i].data.fd, (char *) buffer);
                }
                if (write(events[i].data.fd, buffer, len) == -1) {
                    perror("send error");
                }
                // ev.data.fd = events[i].data.fd;
                // ev.events = EPOLLOUT;
                // epoll_ctl(epfd, EPOLL_CTL_MOD, events[i].data.fd, &ev);
            }
            // else if (events[i].data.fd & EPOLLOUT) {
            //     printf("发送消息: %d\n", events[i].data.fd);
            //     bzero (buffer, sizeof(buffer));
            //     bcopy("The Author@: magicminglee@Hotmail.com", buffer, sizeof("The Author@: magicminglee@Hotmail.com"));
            //     if(send(events[i].data.fd, "Hello,you are connected server proxy 10125!\n", 45,0) == -1) {
            //         perror("send error");
            //     }
            //     ev.data.fd = events[i].data.fd;
            //     epoll_ctl(epfd, EPOLL_CTL_DEL, ev.data.fd, &ev);
            // }
        }
    }
    close(socket_fd);
}
