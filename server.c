#include <string.h>
#include <time.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <netdb.h>
#include "threadpool.h"

#define alloc(type, size) (type *) malloc(sizeof(type)*size)
#define RFC1123FMT "%a, %d %b %Y %H:%M:%S GMT"

//Command line usage: server <port> <pool-size> <max-number-of-request>
char *get_mime_type(char *name);

char *createResponse();

void requestParse(int argc, char *argv[]);

int socketCreation(int, int maxRequests);

int main(int argc, char *argv[]) {
    requestParse(argc, argv);
}


char *get_mime_type(char *name) {
    char *ext = strrchr(name, '.');
    if (!ext) return NULL;
    if (strcmp(ext, ".html") == 0 || strcmp(ext, ".htm") == 0) return "text/html";
    if (strcmp(ext, ".jpg") == 0 || strcmp(ext, ".jpeg") == 0) return "image/jpeg";
    if (strcmp(ext, ".gif") == 0) return "image/gif";
    if (strcmp(ext, ".png") == 0) return "image/png";
    if (strcmp(ext, ".css") == 0) return "text/css";
    if (strcmp(ext, ".au") == 0) return "audio/basic";
    if (strcmp(ext, ".wav") == 0) return "audio/wav";
    if (strcmp(ext, ".avi") == 0) return "video/x-msvideo";
    if (strcmp(ext, ".mpeg") == 0 || strcmp(ext, ".mpg") == 0) return "video/mpeg";
    if (strcmp(ext, ".mp3") == 0) return "audio/mpeg";
    return NULL;
}

void getDate(char *date) {
    time_t now;
    now = time(NULL);
    strftime(date, 128, RFC1123FMT, gmtime(&now));
}

char *createResponse() {
    char *response = alloc(char, 1024);
    char date[128];
    getDate(date);
    strcpy(response,
           "(HTTP/1.0, status, phrase)\r\n"
           "Server: webserver/1.0\r\n"
           "Data: <data> \r\n"
           "Location <url> \r\n" // Only if status is 302
           "Content-Type: <content-type>\r\n"
           "Content-Length: <content-length>\r\n"
           "Last Modified: <date>\r\n"
           "Connection: close\r\n"
           "\r\n");
    return response;
}

void requestParse(int argc, char *argv[]) {
    if (argc != 4) {
        printf("Usage: <port> <pool-size> <max-number-of-request>\n");
        exit(1);
    }
    char *server = argv[0];
    int port = atoi(argv[1]);
    int poolSize = atoi(argv[2]);
    int maxRequests = atoi(argv[3]);
    struct sockaddr_in client;
    int socketFD = socketCreation(port, maxRequests);
    threadpool *pool = create_threadpool(poolSize);
    int serverSockets, clientLength;
    while (1) {
        clientLength = sizeof(struct sockaddr_in);
        if ((serverSockets = accept(socketFD, (struct sockaddr *) &client, (socklen_t *) &clientLength)) < 0) {
            perror("accept");
            exit(1);
        }
        dispatch(pool, (void *) serverSockets, NULL);
    }

}

int socketCreation(int port, int maxRequests) {
    struct sockaddr_in sockIn = {0};
    sockIn.sin_family = AF_INET;
    sockIn.sin_addr.s_addr = htonl(INADDR_ANY);
    sockIn.sin_port = htons(port);
    int fd;
    if ((fd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("socket");
        exit(1);
    }
    int connection;
    if ((connection = bind(fd, (struct sockaddr *) &sockIn, sizeof(sockIn))) < 0) {
        perror("connection");
        exit(1);
    }
    listen(fd, maxRequests);
    return fd;
}