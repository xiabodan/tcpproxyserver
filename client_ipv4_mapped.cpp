/* File Name: client.c */
#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include<errno.h>
#include<unistd.h>
#include<sys/types.h>
#include<sys/socket.h>
#include<netinet/in.h>
#include<arpa/inet.h>

#define MAXLINE 4096

int main(int argc, char** argv) {
    int    sockfd, n,rec_len;
    char    recvline[4096], sendline[4096];
    char    buf[MAXLINE];
    struct sockaddr_in servaddr;


    if ( argc != 2){
        printf("usage: ./client <ipaddress>\n");
        // exit(0);
    }


    if ((sockfd = socket(AF_INET6, SOCK_STREAM, 0)) < 0){
        printf("create socket error: %s(errno: %d)\n", strerror(errno),errno);
        exit(0);
    }

    // memset(&servaddr, 0, sizeof(servaddr));
    // servaddr.sin_family = AF_INET;
    // servaddr.sin_port = htons(10125);
    // if( inet_pton(AF_INET, "172.16.10.239", &servaddr.sin_addr) <= 0){
    //     printf("inet_pton error for %s\n",argv[1]);
    //     exit(0);
    // }

    const char *ipv4_mapped_str ="::FFFF:172.16.10.239";
    in6_addr ipv4_mapped_addr = {0};
    int ipv4_mapped = inet_pton(AF_INET6, ipv4_mapped_str, &ipv4_mapped_addr);
    sockaddr_in6 v4mapped_addr = {0};
    v4mapped_addr.sin6_family = AF_INET6;
    v4mapped_addr.sin6_port = htons(10121);
    v4mapped_addr.sin6_addr = ipv4_mapped_addr;

    if( connect(sockfd, (struct sockaddr*) &v4mapped_addr, sizeof(sockaddr_in6)) < 0){
        printf("connect error: %s(errno: %d)\n",strerror(errno),errno);
        exit(0);
    }

    while (1) {
        printf("send msg to server: \n");
        fgets(sendline, 4096, stdin);
        if( send(sockfd, sendline, strlen(sendline), 0) < 0) {
            printf("send msg error: %s(errno: %d)\n", strerror(errno), errno);
            exit(0);
        }
        if((rec_len = recv(sockfd, buf, MAXLINE,0)) == -1) {
            perror("recv error");
            exit(1);
        }
        buf[rec_len]  = '\0';
        printf("Received : %s ",buf);
    }
    close(sockfd);
    exit(0);
}
