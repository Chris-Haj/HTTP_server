#include <string.h>
#include <time.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <netdb.h>
#include <unistd.h>
#include <stdio.h>
#include <dirent.h>
#include <sys/stat.h>
#include "threadpool.h"
#include <fcntl.h>

#define alloc(type, size) (type *) malloc(sizeof(type)*size)
#define RFC1123FMT "%a, %d %b %Y %H:%M:%S GMT"
#define BYTE 1024
//Command line usage: server <port> <pool-size> <max-number-of-request>
char *get_mime_type(char *name);

char * createResponse(void *SD);

void requestParse(int argc, char *argv[]);

int socketCreation(int, int maxRequests);

int isFile(char *path) {
    struct stat path_stat;
    stat(path, &path_stat);
    return S_ISREG(path_stat.st_mode);
}

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

char * createResponse(void *SD) {
    int socketDescriptor = *((int *) SD);
    char buffer[BYTE] = {0};
    char *response = alloc(char, BYTE*10);
    memset(response,'\0',BYTE);
    char *end;
    while (read(socketDescriptor, buffer, BYTE) > 0)
        if ((end = strstr(buffer, "\r\n")))
            break;
    if (end)
        *end = '\0';
    char date[128];
    getDate(date);
    char *method = strtok(buffer, " ");
    char *path = strtok(NULL, " ");
    char curPath[strlen(path)+2];
    strcpy(curPath,".");
    strcat(curPath,path);
    path = curPath;
    char *protocol = strtok(NULL, "\0");
    int exists = access(path,F_OK);
    DIR *dir = opendir(path);
    int is_file = isFile(path);
    if (strstr(method, "GET") == NULL) {//send 501 not supported (File 501.txt)
        sprintf(response, "HTTP/1.0 501 Not supported\r\n"
                          "Server: webserver/1.0\r\n"
                          "Date: %s\r\n"
                          "Content-Type: text/html\r\n"
                          "Content-Length: 129\r\n"
                          "Connection: close\r\n\r\n"
                          "<HTML><HEAD><TITLE>501 Not supported</TITLE></HEAD>\n"
                          "<BODY><H4>501 Not supported</H4>\n"
                          "Method is not supported.\n"
                          "</BODY></HTML>\n", date);
        printf("501\n");
    } else if (exists == -1) {// 404 Error
        sprintf(response, "HTTP/1.0 404 Not Found\r\n"
                          "Server: webserver/1.0\r\n"
                          "Date: %s\r\n"
                          "Content-Type: text/html\r\n"
                          "Content-Length: 112\r\n"
                          "Connection: close\r\n"
                          "\r\n"
                          "<HTML><HEAD><TITLE>404 Not Found</TITLE></HEAD>\n"
                          "<BODY><H4>404 Not Found</H4>\n"
                          "File not found.\n"
                          "</BODY></HTML>\n", date);
    } else if (is_file == 0 && path[strlen(path) - 1] != '/') { // 302 Error
        sprintf(response, "HTTP/1.0 302 Found\r\n"
                          "Server: webserver/1.0\r\n"
                          "Date: %s\r\n"
                          "Location: %s>\r\n"
                          "Content-Type: text/html\r\n"
                          "Content-Length: 123\r\n"
                          "Connection: close\r\n"
                          "\r\n"
                          "<HTML><HEAD><TITLE>302 Found</TITLE></HEAD>\n"
                          "<BODY><H4>302 Found</H4>\n"
                          "Directories must end with a slash.\n"
                          "</BODY></HTML>\n", path,date);
        printf("302\n");
    } else if (is_file == 0 && path[strlen(path) - 1] == '/') { // Search for index.html in dir
        char indexPath[strlen(path)+strlen("index.html") + 1];
        strcpy(indexPath,path);
        strcat(indexPath,"index.html");
        if (access(indexPath, F_OK) != -1){
            int fd = open(indexPath,O_RDONLY);
            char reader[BYTE] = {0};
            size_t bytesRead=0;
            size_t cur;
            char temp[sizeof(response)]= {0};
            while ((cur = read(fd,reader,BYTE)) > 0){
                strcat(temp,reader);
                bytesRead += cur;
            }
            char *mime = get_mime_type(indexPath);
            sprintf(response, "HTTP/1.0 200 OK\r\n"
                              "Server: webserver/1.0\r\n"
                              "Date: %s\r\n"
                              "Content-Type: %s\r\n"
                              "Content-Length: %zu\r\n"
                              "Connection: close\r\n\r\n", date, mime, bytesRead);
            strcat(response,temp);
        } else {
            sprintf(response, "HTTP/1.0 404 Not Found\r\n"
                              "Server: webserver/1.0\r\n"
                              "Date: %s\r\n"
                              "Content-Type: text/html\r\n"
                              "Content-Length: 112\r\n"
                              "Connection: close\r\n"
                              "\r\n"
                              "<HTML><HEAD><TITLE>404 Not Found</TITLE></HEAD>\n"
                              "<BODY><H4>404 Not Found</H4>\n"
                              "File not found.\n"
                              "</BODY></HTML>\n", date);

        }
    } else if (is_file == 1) { //Path is a file
        //If no read perms, send 403 forbidden error
        //Else return file

    }

    write(socketDescriptor, response, strlen(response));
    close(socketDescriptor);
    return NULL;
}

void requestParse(int argc, char *argv[]) {
    if (argc != 4) {
        printf("Usage: <port> <pool-size> <max-number-of-request>\n");
        exit(1);
    }
    int port = atoi(argv[1]);
    int poolSize = atoi(argv[2]);
    int maxRequests = atoi(argv[3]);
    struct sockaddr_in client;
    int socketFD = socketCreation(port, maxRequests);
    threadpool *pool = create_threadpool(poolSize);
    int serverSockets, clientLength;
    int i = 0;
    while (i < maxRequests) {
        clientLength = sizeof(struct sockaddr_in);
        if ((serverSockets = accept(socketFD, (struct sockaddr *) &client, (socklen_t *) &clientLength)) < 0) {
            perror("accept");
            exit(1);
        }
        dispatch(pool, (void *) createResponse, (void *) &serverSockets);
        i++;
    }
    close(socketFD);
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
    if (bind(fd, (struct sockaddr *) &sockIn, sizeof(sockIn)) < 0) {
        perror("bind");
        exit(1);
    }
    listen(fd, maxRequests);
    return fd;
}