/*
 * proxy.c is a program that acts as an intermediary
 * between clients making requests to access resources
 * and the servers that satisfy those requests by serving content.
 */

/*
 * Documentation of proxy structure:
 * This file is the main file for proxy lab.
 * The first step is that getting the HTTP request message from client.
 * When get the request, finding it in the cache list at first.
 * If cache list contains this request and its response,
 * write the response back to client an close the connect.
 * If cache list does not have the request, rebuild the HTTP
 * header and send it to server.
 * Then stores the response into cache list if the response size
 * is smaller than MAX_OBJECT_SIZE (100 * 1024), and send then message
 * back to client.
 */

#include "cache.h"
#include "csapp.h"

#include <assert.h>
#include <ctype.h>
#include <inttypes.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>

#include <errno.h>
#include <netdb.h>
#include <netinet/in.h>
#include <pthread.h>
#include <signal.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>

/*
 * Debug macros, which can be enabled by adding -DDEBUG in the Makefile
 * Use these if you find them useful, or delete them if not
 */
#ifdef DEBUG
#define dbg_assert(...) assert(__VA_ARGS__)
#define dbg_printf(...) fprintf(stderr, __VA_ARGS__)
#else
#define dbg_assert(...)
#define dbg_printf(...)
#endif

/*
 * Max cache and object sizes
 * You might want to move these to the file containing your cache implementation
 */
#define MAX_CACHE_SIZE (1024 * 1024)
#define MAX_OBJECT_SIZE (100 * 1024)

#define HOSTLEN 256
#define SERVLEN 8
#define MAX_SIZE_HEADER_ELEMENTS 100
#define NAME_MAX_LEN 32
#define VALUE_MAX_LEN 64
#define CACHE_NUMBERS 10

/* Typedef for convenience */
typedef struct sockaddr SA;

/* Information about a connected client. */
typedef struct {
    struct sockaddr_in addr; // Socket address
    socklen_t addrlen;       // Socket address length
    int *connfd;             // Client connection file descriptor
    char host[HOSTLEN];      // Client host
    char serv[SERVLEN];      // Client service (port)
} client_info;

/* Information about request headers */
typedef struct {
    char name[NAME_MAX_LEN];
    char value[VALUE_MAX_LEN * 2];
} ReqHeader;

/*
 * String to use for the User-Agent header.
 * Don't forget to terminate with \r\n
 */
static const char *header_user_agent = "Mozilla/5.0"
                                       " (X11; Linux x86_64; rv:3.10.0)"
                                       " Gecko/20191101 Firefox/63.0.1";

int num; // the number of name-value pair in request header
bool has_host;
cache_block *head;
int left_size;
static pthread_mutex_t mutex;

/*
 * parse_uri - extra host, port and uri from url.
 * url: The url of the HTTP request.
 * host: The host name of the HTTP request.
 * port: The port number of the HTTP request, default is 80.
 * uri: The uri path of the HTTP request.
 */
bool parse_uri(char *url, char *host, char *port, char *uri) {
    char *tmp = strstr(url, "//");
    if (tmp == NULL) {
        tmp = url;
        return false;
    } else
        tmp += 2;

    char *tmp1 = strstr(tmp, "/");
    if (tmp1 == NULL)
        return false;
    char *tmp2 = strstr(tmp, ":");
    if (tmp2 != NULL) {
        strncpy(host, tmp, tmp2 - tmp);
        host[tmp2 - tmp] = '\0';
        tmp2 += 1;
        strncpy(port, tmp2, tmp1 - tmp2);
        port[tmp1 - tmp2] = '\0';
    } else {
        strncpy(host, tmp, tmp1 - tmp);
        host[tmp1 - uri] = '\0';
        strncpy(port, "80", 3);
    }
    strcpy(uri, tmp1);
    return true;
}

/*
 * clienterror - returns an error message to the client
 */
void clienterror(int fd, const char *errnum, const char *shortmsg,
                 const char *longmsg) {
    char buf[MAXLINE];
    char body[MAXBUF];
    size_t buflen;
    size_t bodylen;

    /* Build the HTTP response body */
    bodylen = snprintf(body, MAXBUF,
                       "<!DOCTYPE html>\r\n"
                       "<html>\r\n"
                       "<head><title>Tiny Error</title></head>\r\n"
                       "<body bgcolor=\"ffffff\">\r\n"
                       "<h1>%s: %s</h1>\r\n"
                       "<p>%s</p>\r\n"
                       "<hr /><em>The Tiny Web server</em>\r\n"
                       "</body></html>\r\n",
                       errnum, shortmsg, longmsg);
    if (bodylen >= MAXBUF) {
        return; // Overflow!
    }

    /* Build the HTTP response headers */
    buflen = snprintf(buf, MAXLINE,
                      "HTTP/1.0 %s %s\r\n"
                      "Content-Type: text/html\r\n"
                      "Content-Length: %zu\r\n\r\n",
                      errnum, shortmsg, bodylen);
    if (buflen >= MAXLINE) {
        return; // Overflow!
    }

    /* Write the headers */
    if (rio_writen(fd, buf, buflen) < 0) {
        fprintf(stderr, "Error writing error response headers to client\n");
        return;
    }

    /* Write the body */
    if (rio_writen(fd, body, bodylen) < 0) {
        fprintf(stderr, "Error writing error response body to client\n");
        return;
    }
}

/*
 * read_requesthdrs - read HTTP request headers
 * Returns true if an error occurred, or false otherwise.
 */
bool read_requesthdrs(int connfd, rio_t *rp, ReqHeader *headers,
                      char *hostname) {
    char buf[MAXLINE];
    char name[MAXLINE];
    char value[MAXLINE];
    while (true) {
        if (rio_readlineb(rp, buf, sizeof(buf)) <= 0) {
            return true;
        }

        /* Check for end of request headers */
        if (strcmp(buf, "\r\n") == 0) {
            return false;
        }

        /* Parse header into name and value */
        if (sscanf(buf, "%[^:]: %[^\r\n]", name, value) != 2) {
            /* Error parsing header */
            clienterror(connfd, "400", "Bad Request",
                        "Tiny could not parse request headers");
            return true;
        }

        /* Check if the request header contains Host */
        if (strncasecmp(name, "Host", strlen("Host")) == 0) {
            strcpy(headers[num].name, name);
            strcpy(headers[num].value, value);
            has_host = true;
            num++;
            continue;
        }

        /* Check if the request header contains User-Agent */
        if (strncasecmp(name, "User-Agent", strlen("User-Agent")) == 0) {
            continue;
        }

        /* Check if the request header contains Connection */
        if (strncasecmp(name, "Connection", strlen("Connection")) == 0) {
            continue;
        }

        /* Check if the request header contains Proxy-Connection */
        if (strncasecmp(name, "Proxy-Connection", strlen("Proxy-Connection")) ==
            0) {
            continue;
        }

        /* store extra information in the headers array */
        strcpy(headers[num].name, name);
        strcpy(headers[num].value, value);
        num++;
        printf("%s: %s\n", name, value);
    }
}

/*
 * build_header - build the header for fature server request.
 * filepath - uri
 * headers - headers store the Http request header's information
 * new_header - header of server request
 */
void build_header(char *filepath, ReqHeader *headers, char *new_header) {
    char *buf = new_header;
    sprintf(buf, "GET %s HTTP/1.0\r\n", filepath);
    buf = new_header + strlen(new_header);
    /* Add all information from headers to new_header */
    for (int i = 0; i < num; i++) {
        sprintf(buf, "%s: %s\r\n", headers[i].name, headers[i].value);
        buf = new_header + strlen(new_header);
    }
    sprintf(buf, "\r\n");
}

/*
 * do_it - extra infromation from Http request, handle errors, send to server.
 * Get the responds from server, and send it back to client.
 */
void do_it(int connfd) {

    rio_t rio;
    rio_readinitb(&rio, connfd);
    ReqHeader headers[40];

    /* Read request line */
    char buf[MAXLINE];
    if (rio_readlineb(&rio, buf, sizeof(buf)) <= 0) {
        return;
    }

    /* Parse the request line and check if it's well-formed */
    char method[MAXLINE];
    char uri[MAXLINE];
    char version;

    /* sscanf must parse exactly 3 things for request line to be well-formed */
    /* version must be either HTTP/1.0 or HTTP/1.1 */
    if (sscanf(buf, "%s %s HTTP/1.%c", method, uri, &version) != 3 ||
        (version != '0' && version != '1')) {
        clienterror(connfd, "400", "Bad Request",
                    "Tiny received a malformed request");
        return;
    }

    /* Check that the method is GET */
    if (strcmp(method, "GET") != 0) {
        clienterror(connfd, "501", "Not Implemented",
                    "Tiny does not implement this method");
        return;
    }

    int ret;
    pthread_mutex_lock(&mutex);
    ret = read_cache(uri, connfd, head);
    pthread_mutex_unlock(&mutex);
    fprintf(stderr, "zyzyz: %d\n", ret);
    if (ret != -1) {
        return;
    }

    /*copy uri to use as the key of cache object*/
    char *copy_uri = (char *)Malloc(sizeof(uri));
    memcpy(copy_uri, uri, sizeof(uri));

    /* Parse URI from GET request */
    char filepath[MAXLINE];
    char hostname[MAXLINE], port[MAXLINE];
    if (!parse_uri(uri, hostname, port, filepath)) {
        clienterror(connfd, "400", "Bad Request",
                    "Tiny received a malformed request");
        return;
    }

    num = 0;
    has_host = false;
    /* Check if reading request headers caused an error */
    if (read_requesthdrs(connfd, &rio, headers, hostname)) {
        return;
    }
    /* Add Host, User-Agent, Connection and Proxy-Connection to headers */
    if (!has_host) {
        strcpy(headers[num].name, "Host");
        strcpy(headers[num].value, hostname);
        num++;
    }
    strcpy(headers[num].name, "User-Agent");
    strcpy(headers[num].value, header_user_agent);
    num++;
    strcpy(headers[num].name, "Connection");
    strcpy(headers[num].value, "close");
    num++;
    strcpy(headers[num].name, "Proxy-Connection");
    strcpy(headers[num].value, "close");
    num++;
    char new_header[MAXBUF];

    /* Build server request header */
    build_header(filepath, headers, new_header);

    fprintf(stderr, "%s", new_header);

    int serverfd;
    serverfd = open_clientfd(hostname, port);

    rio_t server_rio;
    rio_readinitb(&server_rio, serverfd);
    // send client header to real server
    if (rio_writen(serverfd, new_header, MAXLINE) < 0) {
        fprintf(stderr, "Error writing static response headers to client\n");
        close(serverfd);
        return;
    }

    // rio_writen(serverfd, new_header, MAXLINE);
    int size = 0;
    int n;
    char object[MAX_OBJECT_SIZE];
    char *location = object;
    int large_flag = 0;
    // server response to buf
    while ((n = rio_readnb(&server_rio, buf, MAXLINE))) {
        if ((size + n) <= MAX_OBJECT_SIZE && large_flag == 0) {
            memcpy(location, buf, n);
            location += n;
            size += n;
        } else {
            large_flag = 1;
        }
        rio_writen(connfd, buf, n);
    }
    fprintf(stderr, "large_flag: %d, size: %d\n", large_flag, size);
    if (large_flag == 0) {
        // write_cache(copy_uri, object, size);
        pthread_mutex_lock(&mutex);
        left_size = insert_cache(copy_uri, object, size, head, left_size);
        pthread_mutex_unlock(&mutex);
        // insert_cache(copy_uri, object, size, head, &left_size);
        fprintf(stderr, "left_size: %d\n", left_size);
    }
    close(serverfd);
}

/* Thread routine */
void *thread(void *vargp) {
    int connfd = *((int *)vargp);
    pthread_detach(pthread_self());
    Free(vargp);
    do_it(connfd);
    close(connfd);
    return NULL;
}

/*
 * main - main function.
 */
int main(int argc, char **argv) {
    int listenfd;
    pthread_t tid;
    signal(SIGPIPE, SIG_IGN);
    char hostname[MAXLINE], port[MAXLINE];
    /* Check command line args */
    if (argc != 2) {
        fprintf(stderr, "usage: %s <port>\n", argv[0]);
        exit(1);
    }
    listenfd = open_listenfd(argv[1]);
    if (listenfd < 0) {
        fprintf(stderr, "Failed to listen on port: %s\n", argv[1]);
        exit(1);
    }

    // init_cache();
    head = init_cache(head);
    pthread_mutex_init(&mutex, NULL);
    left_size = MAX_CACHE_SIZE;
    while (1) {
        /* Allocate space on the stack for client info */
        client_info client_data;
        client_info *client = &client_data;

        /* Initialize the length of the address */
        client->addrlen = sizeof(client->addr);

        /* accept() will block until a client connects to the port */
        client->connfd = Malloc(sizeof(int));
        *client->connfd =
            accept(listenfd, (SA *)&client->addr, &client->addrlen);

        getnameinfo((SA *)&client->addr, client->addrlen, hostname, MAXLINE,
                    port, MAXLINE, 0);

        if (client->connfd < 0) {
            perror("accept");
            continue;
        }

        /* Connection is established; serve client */
        pthread_create(&tid, NULL, thread, client->connfd);
    }
    // free_cache();
    free_cache(head);
}
