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
#include <sys/syscall.h>

#define DEFAULT_PORT 10125
#define MAXLINE 4096
#define MAXFDS 256
#define EVENTS 100
#define gettid() syscall(SYS_gettid)

struct thread_arg {
    int cfd;
    void* caddr;
} thread_arg;

void *thread_client(void *); 
int client(void);

void *thread_client(void *junk) {
    printf("thread_client tid %d\n", gettid());
    while(1) {
        sleep(1);
    }
}

int main() {
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
    int on = SO_REUSEADDR;
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

    printf("======waiting for client's request======\n");

    while (1) {
        int cfd = accept(socket_fd, (struct sockaddr*)NULL, NULL);
        if (-1 == cfd) {
            printf("服务器接收套接字的时候出问题了");
        }
        printf("accept cfd %d, tid %d\n", cfd, gettid());

        pthread_t t_a;
        pthread_create(&t_a, NULL, thread_client, (void*) NULL);
    }

    return 0;
}
