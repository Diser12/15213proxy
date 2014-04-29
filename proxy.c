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

int doproxy(int connfd);
int read_request(int connfd, rio_t *rp, char *bufrequest, char *hostname, int *port, char *uri);
int parse_uri(char *uri, char *hostname, int *port);

void echo(int connfd);
void *thread(void *vargp);
void parse(int connfd, char *uri, char *hostname, int *port);

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
        //parse(*connfd);
        Pthread_create(&tid, NULL, thread, connfd);
        Pthread_join(tid, NULL);
    }
    return 0;
}

int doproxy(int connfd)
{
    printf("doproxy in!\n");
    int port, serverfd, len, response_len;
    char bufrequest[MAXLINE], bufresponse[MAXLINE], hostname[MAXLINE], uri[MAXLINE];
    rio_t rio_client, rio_server;

    // reset buffer
    memset(bufrequest, 0, sizeof(bufrequest));
    memset(bufresponse, 0, sizeof(bufresponse));
    memset(hostname, 0, sizeof(hostname));
    memset(uri, 0, sizeof(uri));

    /* Read request line and headers */
    rio_readinitb(&rio_client, connfd);
    // bufrequest is the buffer for outgoing request to real server
    //printf("read_request in!\n");
        
    if(read_request(connfd, &rio_client, bufrequest, hostname, &port, uri) < 0){
        //printf("read request < 0");
        return -1;
    }
    //printf("read_request out!\n");
    printf("uri:%s\thostname:%s\tport:%d\n", uri, hostname, port);
    printf("bufrequest: %s\n", bufrequest);

    if((serverfd = open_clientfd_r(hostname, port)) < 0)
        {
            fprintf(stderr, "open server fd error\n");
            return -1;
        }
    rio_readinitb(&rio_server, serverfd);

    if(rio_writen(serverfd, bufrequest, strlen(bufrequest)) < 0)
    {
        printf("bufrequest error:\n%s\n", bufrequest);
        fprintf(stderr, "rio_writen send request error\n");
        close(serverfd);    
    // send back error msg to client
    //clienterror(connfd, "request error", "404", "Not found", "Send request to server error");

    return -1;
    }

    // get response from server and send back to the client
    response_len = 0;
    while((len = rio_readnb(&rio_server, bufresponse, sizeof(bufresponse))) > 0)
    {
        printf("hostname:%s\tport:%d\nbufresponse:%s\n", hostname, port, bufresponse);
        response_len += len;

        // TODO: n error!
            if(rio_writen(connfd, bufresponse, len) < 0)
            {
                printf("bufresponse error:%s\nlen:%d\n%s\n", strerror(errno), len, bufresponse); 
                fprintf(stderr, "rio_writen send response error\n");
                //clienterror(connfd, "response error", "404", "Not found", "Send response to client error");
                close(serverfd);
            return -1;
            }
        // reset buffer
        memset(bufresponse, 0, sizeof(bufresponse));
    }
    // close fd
    close(serverfd);

    return 0;
}

int read_request(int connfd, rio_t *rio, char *bufrequest, char *hostname, int *port, char *uri)
{
    char buf[MAXLINE], method[MAXLINE];
    memset(buf, 0, sizeof(buf));
    memset(method, 0, sizeof(method));
    // request line
    rio_readlineb(rio, buf, MAXLINE);
    printf("buf: %s\n", buf);
    sscanf(buf, "%s %s", method, uri);
    if (strcmp(method, "GET")) return -1;
    printf("uri: %s\n", uri);
    // extract hostname and port info
    parse(connfd, uri, hostname, port);
    // fill in request for real server
    sprintf(bufrequest, "%s %s HTTP/1.0\r\n", method, uri);
    printf("bufrequest: %s\n", bufrequest);
        
    // request hdr
    if(rio_readlineb(rio, buf, MAXLINE) < 0)
    {
        printf("rio_readlineb fail!\n");
        return -1;
    }
    while(strcmp(buf, "\r\n")) {
        //printf("buf: %s\n", buf);
        //printf("while loop\n");
        if(strcmp(buf, "\n") == 0)
        {
            printf("hahaha!\n");
        }
        if(strcmp(buf, "\r") == 0)
        {
            printf("lalala!\n");
        }
        if(strstr(buf, "Host"))
        {
            strcat(bufrequest, "Host: ");
            strcat(bufrequest, hostname);
            strcat(bufrequest, "\r\n");
        }
        else if(strstr(buf, "Accept:"))
                strcat(bufrequest, accept_hdr);
        else if(strstr(buf, "Accept-Encoding:"))
                strcat(bufrequest, accept_encoding_hdr);
        else if(strstr(buf, "User-Agent:"))
                strcat(bufrequest, user_agent_hdr);
        else if(strstr(buf, "Proxy-Connection:"))
                strcat(bufrequest, "Proxy-Connection: close\r\n");
        else if(strstr(buf, "Connection:"))
                strcat(bufrequest, "Connection: close\r\n");
        else if(!strstr(buf, "Cookie:"))
        // append addtional header except above listed headers
        strcat(bufrequest, buf);

        memset(buf, 0, sizeof(buf));
            if(rio_readlineb(rio, buf, MAXLINE) < 0)
            {
                    fprintf(stderr, "rio_readlineb read request error\n");
                    return -1;
            }
        }
    
        //append header separater
    strcat(bufrequest, "\r\n");

    return 0;
}

void *thread(void *vargp)
{
    int connfd = *((int *)vargp);   
    Pthread_detach(pthread_self());    
    Free(vargp);
    doproxy(connfd);   
    Close(connfd); 
    return NULL;
}

void parse(int connfd, char *uri, char *hostname, int *port)
{
    char host[MAXLINE], path[MAXLINE], portHolder[MAXLINE];
    int lenHost, portExists;

    //host, full path, and port
    //printf("uri = %s\n", uri);
    strcpy(host, uri + strlen("http://"));
    portExists = ((int)strcspn(host, "/") < (int)strcspn(host, ":"));
    if(portExists)
        lenHost = strcspn(host, "/");
    else lenHost = strcspn(host, ":");
    strncpy(hostname, host, lenHost);
    //printf("final host = %s\n", hostname);
    if(portExists){
        strcpy(path, host + lenHost);
        *port = 80;
    }
    else{
        strncpy(portHolder, host + lenHost, strcspn(host + lenHost, "/"));
        strcpy(path, host + lenHost + strlen(portHolder));
        memmove(portHolder, portHolder + 1, strlen(portHolder));
        *port = atoi(portHolder);
    }
    strcat(path, " HTTP/1.0");
    //printf("full path = %s\nport = %d\n", path, *port);
    //printf("uri = %s\n", uri);
}
