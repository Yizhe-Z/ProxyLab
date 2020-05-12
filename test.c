// #include <assert.h>
// #include <ctype.h>
// #include <inttypes.h>
// #include <stdbool.h>
// #include <stdio.h>
// #include <stdlib.h>
// #include <string.h>
// #include <strings.h>
// #include <unistd.h>

// #include <errno.h>
// #include <netdb.h>
// #include <netinet/in.h>
// #include <pthread.h>
// #include <signal.h>
// #include <sys/socket.h>
// #include <sys/types.h>

// void parse_uri(char *uri, char *host, char *port, char *url, int *a) {
//     if (strstr(uri, "http") == uri) {
//         uri += strlen("http://");
//     } else {
//         printf("Error\n");
//     }
//     char *tmp1 = strstr(uri, "/");
//     char *tmp2 = strstr(uri, ":");
//     if (tmp2 != NULL) {
//         strncpy(host, uri, tmp2 - uri);
//         host[tmp2 - uri] = '\0';
//         tmp2 += 1;
//         strncpy(port, tmp2, tmp1 - tmp2);
//         port[tmp1 - tmp2] = '\0';
//     } else {
//         strncpy(host, uri, tmp1 - uri);
//         host[tmp1 - uri] = '\0';
//         strncpy(port, "80", 3);
//     }
//     strcpy(url, tmp1);
//     *a++;
// }

// int main() {
//     char host[80];
//     char port[80];
//     char url[80];
//     char uri[80] = "http://cmu.edu:8080/zyz";
//     int a = 0;
//     parse_uri(uri, host, port, url, &a);
//     // printf("%s\n",url);
//     // printf("%s\n",host);
//     // printf("%s\n",port);
//     // printf("%d\n",a);
//     char *ans[9999999];
//     char *x[100];
//     char *y[100];
//     char *z[100];
//     strcpy(x, "xxx");
//     sprintf(ans, "%s\r\n", x);
//     printf("%s", ans);
//     return 0;
// }
