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
#include <arpa/inet.h>
#include <sys/epoll.h>
#include <iostream>
#include <fcntl.h>
#include <signal.h>
#include <sys/syscall.h>

#define DEFAULT_PORT 10125
#define MAXLINE 4096
#define MAXFDS 10
#define EVENTS 10
#define gettid() syscall(SYS_gettid)

static bool debug = true;

struct thread_arg {
    int cfd;
    void* caddr;
} thread_arg;

void *thread_client(void *);
int client(void);
int create_remote_socket(int port, const char* target_ip);
bool setNonBlock(int fd);
void close_fd_safety(int fd);

struct protocol {
    char magic[10];
    unsigned short port;
    int iplen;
} protocol;

void *thread_client(void *arg_) {
    char buffer[MAXLINE];
    struct thread_arg* arg = (struct thread_arg*) arg_;
    int cfd = arg->cfd;

    int remote = 0;
    printf("proxy listen thread started tid %d, cfd %d\n", gettid(), cfd);
    bzero(buffer, sizeof(buffer));
    int total = 0;
    if ((total = recv(cfd, buffer, 128, 0)) > 0) {
        struct protocol* pro = (struct protocol*) buffer;
        if (!strncmp(pro->magic, "##**##**55", 10)) {
            printf("接收到客户端 cfd %d 连接请求: %d, %d, %d, %s\n", cfd, total, pro->port, pro->iplen, &buffer[sizeof(struct protocol)]);
            remote = create_remote_socket(pro->port, &buffer[sizeof(struct protocol)]);
            if (remote <= 0) {
                printf("remote server is not running.");
                close_fd_safety(cfd);
                return;
            }
        }
    } else if (total == 0) {
        printf("connect disconnected from client %d\n", cfd);
    } else {
        printf("recv client addr and port error: %s(errno: %d)\n", strerror(errno), errno);
        close_fd_safety(cfd);
        return;
    }

    // 创建epoll
    int nfds;
    struct epoll_event ev, ev2, events[EVENTS];
    int epfd = epoll_create(MAXFDS);

    setNonBlock(cfd);
    ev.data.fd = cfd;
    ev.events = EPOLLIN | EPOLLET;
    epoll_ctl(epfd, EPOLL_CTL_ADD, cfd, &ev);

    setNonBlock(remote);
    ev2.data.fd = remote;
    ev2.events = EPOLLIN | EPOLLET;
    epoll_ctl(epfd, EPOLL_CTL_ADD, remote, &ev2);

    bool return_flag = false;
    while (1) {
        nfds = epoll_wait(epfd, events, MAXFDS, 2000);
        if (nfds > 0) {
            // printf("epoll_wait respond nfds %d %d------\n", nfds, events[0].data.fd);
        }
        for (int i = 0; i < nfds; i++) {
            if (events[i].events == EPOLLIN) {
                bool from_client = false;
                if (events[i].data.fd == cfd) {
                    from_client = true;
                }
                bzero(buffer, sizeof(buffer));
                int len = recv(events[i].data.fd, buffer, sizeof(buffer), 0);
                if (len == 0) {
                    printf("connect disconnected %d\n", events[i].data.fd);
                    return_flag = true;
                    break;
                } else if (len < 0) {
                    printf("connect_fd recv error: %s(errno: %d)\n", strerror(errno), errno);
                    continue;
                } else {
                    if (from_client) {
                        if (debug) printf("read  fd %d client->proxy len: %d\n", events[i].data.fd, len);
                    } else {
                        if (debug) printf("read  fd %d proxy->client len: %d\n", events[i].data.fd, len);
                    }
                }
                int fd = cfd;
                if (events[i].data.fd == cfd) {
                    fd = remote;
                }
                if (len > 0) {
                    len = write(fd, buffer, len);
                    if (from_client) {
                        if (debug) printf("write fd %d proxy->server len: %d\n", fd, len);
                    } else {
                        if (debug) printf("write fd %d server->proxy len: %d\n", fd, len);
                    }
                }
                // ev.data.fd = events[i].data.fd;
                // ev.events = EPOLLOUT;
                // epoll_ctl(epfd, EPOLL_CTL_MOD, events[i].data.fd, &ev);
            }
        }
        if (return_flag) {
            break;
        }
    }
    close_fd_safety(remote);
    close_fd_safety(cfd);
}

void close_fd_safety(int fd) {
    if (fd > 0) {
        close(fd);
    }
}

int start_client(int cfd, struct sockaddr_in* caddr) {
    printf("start_client cfd %d", cfd);
    struct thread_arg *arg = (struct thread_arg*) malloc(sizeof(struct thread_arg));
    arg->cfd = cfd;
    arg->caddr = caddr;

    pthread_t t_a;
    pthread_create(&t_a, NULL, thread_client, arg);
    pthread_join(t_a, NULL);
}

bool setNonBlock(int fd) {
    int flags = fcntl(fd, F_GETFL, 0);
    flags |= O_NONBLOCK;
    if(-1 == fcntl(fd, F_SETFL, flags))
        return false;
    return true;
}

int create_remote_socket(int port, const char* target_ip) {
    int sockfd;
    struct sockaddr_in servaddr;

    printf("proxy server connecting to remote port %s:%d\n", target_ip, port);

    if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        printf("create remote socket error: %s(errno: %d)\n", strerror(errno),errno);
        return 0;
    }

    memset(&servaddr, 0, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_port = htons(port);
    if (inet_pton(AF_INET, target_ip, &servaddr.sin_addr) <= 0) {
        printf("create remote socket inet_pton error for %s\n", target_ip);
        return 0;
    }
    if (connect(sockfd, (struct sockaddr*)&servaddr, sizeof(servaddr)) < 0) {
        printf("connect remote error: %s(errno: %d)\n", strerror(errno), errno);
        return 0;
    }
    printf("proxy server connected to remote sockfd %d\n", sockfd);

    return sockfd;
}

int creat_socket(bool proxy, int port, char* target_ip) {
    int    socket_fd, connect_fd;
    struct sockaddr_in servaddr, caddr;
    char    buff[4096];
    int     n;
    //初始化Socket
    if ((socket_fd = socket(AF_INET, SOCK_STREAM, 0)) == -1 ) {
        printf("create socket error: %s(errno: %d)\n",strerror(errno),errno);
        return 0;
    }
    /* Enable address reuse */
    int on = 1;
    setsockopt(socket_fd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on));
    //初始化
    memset(&servaddr, 0, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    if (proxy) {
        servaddr.sin_addr.s_addr = htonl(INADDR_ANY);//IP地址设置成INADDR_ANY,让系统自动获取本机的IP地址。
    } else {
        servaddr.sin_addr.s_addr = inet_addr(target_ip);
    }
    servaddr.sin_port = htons(port);

    //将本地地址绑定到所创建的套接字上
    if (bind(socket_fd, (struct sockaddr*)&servaddr, sizeof(servaddr)) == -1) {
        printf("bind socket error: %s(errno: %d)\n",strerror(errno),errno);
        return 0;
    }
    //开始监听是否有客户端连接
    if (listen(socket_fd, 10) == -1){
        printf("listen socket error: %s(errno: %d)\n",strerror(errno),errno);
        return 0;
    }
    printf("listen socket_fd %d\n", socket_fd);

    return socket_fd;
}

int main(int argc, char** argv) {
    struct sockaddr_in caddr;
    int socket_fd = creat_socket(true, DEFAULT_PORT, NULL);

    printf("======waiting for client's request======\n");
    int len = sizeof(caddr);

    // create proxy server accept epoll
    int nfds;
    struct epoll_event ev, ev2, events[EVENTS];
    int epfd = epoll_create(MAXFDS);

    setNonBlock(socket_fd);
    ev.data.fd = socket_fd;
    ev.events = EPOLLIN | EPOLLET;
    epoll_ctl(epfd, EPOLL_CTL_ADD, socket_fd, &ev);

    while (1) {
        nfds = epoll_wait(epfd, events, MAXFDS, -1);
        // printf("epoll_wait socket_fd respond nfds %d\n", nfds);
        for (int i = 0; i < nfds; ++i) {
            if (socket_fd == events[i].data.fd) {
                memset(&caddr, 0, sizeof(caddr) );
                int cfd = accept(socket_fd, (struct sockaddr *) &caddr, &len);
                if (-1 == cfd) {
                    printf("proxy server accept error!");
                    break;
                }
                struct thread_arg *arg = (struct thread_arg*) malloc(sizeof(struct thread_arg));
                arg->cfd = cfd;
                arg->caddr = &caddr;
                pthread_t t_a;
                pthread_create(&t_a, NULL, thread_client, arg);
            }
        }
        // int cfd = accept(socket_fd, (struct sockaddr *) &caddr, &len);
        // if (-1 == cfd) {
        //     printf("proxy server accept error!");
        //     break;
        // }
        // struct thread_arg *arg = (struct thread_arg*) malloc(sizeof(struct thread_arg));
        // arg->cfd = cfd;
        // arg->caddr = &caddr;
        // pthread_t t_a;
        // pthread_create(&t_a, NULL, thread_client, arg);
    }
    close_fd_safety(socket_fd);
    return 0;
}
