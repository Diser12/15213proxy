#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "csapp.h"

/* Recommended max cache and object sizes */
#define MAX_CACHE_SIZE 1049000
#define MAX_OBJECT_SIZE 102400

/* You won't lose style points for including these long lines in your code */
static const char *user_agent_hdr = "User-Agent: Mozilla/5.0 (X11; Linux x86_64; rv:10.0.3) Gecko/20120305 Firefox/10.0.3\r\n";
static const char *accept_hdr = "Accept: text/html,application/xhtml+xml,application/xml;q=0.9,*/*;q=0.8\r\n";
static const char *accept_encoding_hdr = "Accept-Encoding: gzip, deflate\r\n";

void parse(int connfd);

int main(int argc, char **argv)
{
    int port, listenfd, connfd, clientlen;
    struct sockaddr_in clientaddr;
    if (argc != 2) {
            fprintf(stderr, "usage: %s <port>\n", argv[0]);
            exit(0);
        }
    port = atoi(argv[1]);

    printf("\n%d \n\n%s \n%s \n%s\n", 
        port, user_agent_hdr, accept_hdr, accept_encoding_hdr);
    listenfd = Open_listenfd(port);
    while (1) {
        clientlen = sizeof(clientaddr);
        connfd = Accept(listenfd, (SA *)&clientaddr, &clientlen);
        parse(connfd);
        Close(connfd);
    }
    return 0;
}

void parse(int connfd)
{
    char buf[MAXLINE], method[MAXLINE], uri[MAXLINE], version[MAXLINE];
    char host[MAXLINE], finalhost[MAXLINE], path[MAXLINE];
    int lenHost;
    rio_t rio;

    Rio_readinitb(&rio, connfd);
    Rio_readlineb(&rio, buf, MAXLINE);
    sscanf(buf, "%s %s %s", method, uri, version);

    //host and full path 
    if(strncmp("http://", uri, 7) == 0){
        strcpy(host, uri + 7);
        lenHost = (int)strcspn(host, "/");
        strncpy(finalhost, host, lenHost);
        printf("final host = %s\n", finalhost);
        strcpy(path, host + lenHost);
        printf("path = %s\n", path);
        strcat(path, " ");
        strcat(path, version);
        printf("full path = %s\n", path);
    }
    else printf("not http://\n");

    printf("method = %s, uri = %s, version = %s\n", method, uri, version);
}
