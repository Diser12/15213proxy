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

void echo(int connfd);
void *thread(void *vargp);
void parse(int connfd);

int main(int argc, char **argv)
{
    int port, listenfd, *connfd;
    socklen_t clientlen = sizeof(struct sockaddr_in);
    struct sockaddr_in clientaddr;
    pthread_t tid;

    if (argc != 2) {
            fprintf(stderr, "usage: %s <port>\n", argv[0]);
            exit(0);
        }
    port = atoi(argv[1]);

    printf("\n%s \n%s \n%s\n", 
        user_agent_hdr, accept_hdr, accept_encoding_hdr);
    listenfd = Open_listenfd(port);
    while (1) {
        connfd = Malloc(sizeof(int));
        *connfd = Accept(listenfd, (SA *)&clientaddr, &clientlen);
        parse(*connfd);
        Pthread_create(&tid, NULL, thread, connfd);
        Pthread_join(tid, NULL);
    }
    return 0;
}

void echo(int connfd)
{
    size_t n; 
    char buf[MAXLINE]; 
    rio_t rio;

    Rio_readinitb(&rio, connfd);
    while((n = Rio_readlineb(&rio, buf, MAXLINE)) != 0) { //line:netp:echo:eof
        printf("server received %d bytes\n", (int)n);
        Rio_writen(connfd, buf, n);
    }
}

void *thread(void *vargp)
{
    int connfd = *((int *)vargp);   
    Pthread_detach(pthread_self());    
    Free(vargp);
    echo(connfd);   
    Close(connfd); 
    return NULL;
}

void parse(int connfd)
{
    char buf[MAXLINE], method[MAXLINE], uri[MAXLINE], version[MAXLINE];
    char host[MAXLINE], finalhost[MAXLINE], path[MAXLINE], port[MAXLINE];
    int lenHost, portExists;
    rio_t rio;

    Rio_readinitb(&rio, connfd);
    Rio_readlineb(&rio, buf, MAXLINE);
    sscanf(buf, "%s %s %s", method, uri, version);
    if (strcmp("GET", method))
        printf("not http://\n");

    //host, full path, and port
    else {
        strcpy(host, uri + strlen("http://"));
        portExists = ((int)strcspn(host, "/") < (int)strcspn(host, ":"));
        if(portExists)
            lenHost = strcspn(host, "/");
        else lenHost = strcspn(host, ":");
        strncpy(finalhost, host, lenHost);
        printf("final host = %s\n", finalhost);
        if(portExists){
            strcpy(path, host + lenHost);
            strcpy(port, "80");
        }
        else{
            strncpy(port, host + lenHost, strcspn(host + lenHost, "/"));
            strcpy(path, host + lenHost + strlen(port));
            memmove(port, port + 1, strlen(port));
        }
        strcat(path, " ");
        strcat(path, version);
        printf("full path = %s\nport = %s\n", path, port);
    }

    printf("method = %s, uri = %s, version = %s\n", method, uri, version);
}
