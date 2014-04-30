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

sem_t mutex;

void *thread(void *vargp);
int doit(int connfd);
int read_request(int connfd, rio_t *rp, char *buf_request, char *host, int *port, char *uri);
void parse(char *uri, char *host, int *port);

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
 
    // Ignore SIGPIPE
    signal(SIGPIPE, SIG_IGN);

    printf("%s %s %s\n", user_agent_hdr, accept_hdr, accept_encoding_hdr);
    
    sem_init(&mutex, 0, 1);
    
    listenfd = Open_listenfd(port);
    while (1) {
        connfd = Malloc(sizeof(int));
        *connfd = Accept(listenfd, (SA *)&clientaddr, &clientlen);
        Pthread_create(&tid, NULL, thread, connfd);
        Pthread_join(tid, NULL);
    }
    return 0;
}

/* thread:
 *  What each thread does:
 *      Kills itself when done, locks itself into doit until finished,
 *      then unblocks itself.
 */
void *thread(void *vargp)
{
    int connfd = *((int *)vargp);   
    Pthread_detach(pthread_self());    
    Free(vargp);
    P(&mutex);
    doit(connfd);
    V(&mutex);
    Close(connfd); 
    return NULL;
}

/* doit:
 *  Handles the actual communication between the client and the server
 *  Takes in the request of the client, parses it out, then sends the 
 *  clients request to the server. The server is then accessed and the 
 *  server sends all of the information back to the client that it is 
 *  able to.
 */
int doit(int connfd)
{
    int port, serverfd, len, response_len;
    char buf_request[MAXLINE], buf_response[MAXLINE];
    char host[MAXLINE], uri[MAXLINE];
    rio_t rio_client, rio_server;

    // reset buffer and set to 0
    memset(buf_request, 0, sizeof(buf_request));
    memset(buf_response, 0, sizeof(buf_response));
    memset(host, 0, sizeof(host));
    memset(uri, 0, sizeof(uri));

    // read request line and headers
    rio_readinitb(&rio_client, connfd);
    
    // bufrequest is the buffer for our request to the server
    if(read_request(connfd, &rio_client, buf_request, host, &port, uri) < 0) {
        return -1;
    }
    
    // connect to the server
    if((serverfd = open_clientfd_r(host, port)) < 0) {
            fprintf(stderr, "open server fd error\n");
            return -1;
    }
    
    // read response line and headers
    rio_readinitb(&rio_server, serverfd);

    if(rio_writen(serverfd, buf_request, strlen(buf_request)) < 0) {
        fprintf(stderr, "rio_writen send request error\n");
        close(serverfd);    
        return -1;
    }

    // get response from server and send back to the client
    response_len = 0;
    while((len = rio_readnb(&rio_server, buf_response, sizeof(buf_response))) > 0) {
        response_len += len;

        if(rio_writen(connfd, buf_response, len) < 0) {
            fprintf(stderr, "rio_writen send response error\n");
            close(serverfd);
            return -1;
        }
        // empty the buffer
        memset(buf_response, 0, sizeof(buf_response));
    }
    close(serverfd);

    return 0;
}

/* read_request:
 *  The request is read and put into a buffer to be parsed, then puts all
 *  the important data from the request together into bufrequest.
 */
int read_request(int connfd, rio_t *rio, char *buf_request, char *host, int *port, char *uri)
{
    char buf[MAXLINE], method[MAXLINE];
    // set our buffer and method to 0
    memset(buf, 0, sizeof(buf));
    memset(method, 0, sizeof(method));
    // request line
    rio_readlineb(rio, buf, MAXLINE);
    printf("buf: %s\n", buf);
    sscanf(buf, "%s %s", method, uri);
    if (strcmp(method, "GET")) 
        return -1;
    printf("uri: %s\n", uri);
    // parse to get hostname and port info
    parse(uri, host, port);
    // fill in the request to send to server
    sprintf(buf_request, "%s %s HTTP/1.0\r\n", method, uri);
    printf("bufrequest: %s\n", buf_request);
        
    // request hdr
    if(rio_readlineb(rio, buf, MAXLINE) < 0)
    {
        printf("rio_readlineb fail!\n");
        return -1;
    }
    while(strcmp(buf, "\r\n")) {
        if(strstr(buf, "Host")) {
            strcat(buf_request, "Host: ");
            strcat(buf_request, host);
            strcat(buf_request, "\r\n");
        }
        else if(strstr(buf, "Accept:"))
            strcat(buf_request, accept_hdr);
        else if(strstr(buf, "Accept-Encoding:"))
            strcat(buf_request, accept_encoding_hdr);
        else if(strstr(buf, "User-Agent:"))
            strcat(buf_request, user_agent_hdr);
        else if(strstr(buf, "Proxy-Connection:"))
            strcat(buf_request, "Proxy-Connection: close\r\n");
        else if(strstr(buf, "Connection:"))
            strcat(buf_request, "Connection: close\r\n");
        else if(!strstr(buf, "Cookie:"))
            strcat(buf_request, buf);

        //empty buffer
        memset(buf, 0, sizeof(buf));
        if(rio_readlineb(rio, buf, MAXLINE) < 0) {
            fprintf(stderr, "rio_readlineb read request error\n");
            return -1;
        }
    }
    
    //add header separater
    strcat(buf_request, "\r\n");

    return 0;
}

/* parse: 
 *  takes in the uri and separates the hostname and port from it
 *  and puts them into their respective pointers.
 */
void parse(char *uri, char *host, int *port)
{
    char hostname[MAXLINE], path[MAXLINE], portHolder[MAXLINE];
    int lenHost, portExists;

    strcpy(hostname, uri + strlen("http://"));
    portExists = ((int)strcspn(hostname, "/") < (int)strcspn(hostname, ":"));
    if(portExists)
        lenHost = strcspn(hostname, "/");
    else lenHost = strcspn(hostname, ":");
    strncpy(host, hostname, lenHost);
    if(portExists) {
        strcpy(path, hostname + lenHost);
        *port = 80;
    }
    else {
        strncpy(portHolder, hostname + lenHost, strcspn(hostname + lenHost, "/"));
        strcpy(path, hostname + lenHost + strlen(portHolder));
        memmove(portHolder, portHolder + 1, strlen(portHolder));
        *port = atoi(portHolder);
    }
    strcat(path, " HTTP/1.0");
}
