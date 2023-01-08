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
int checkValid(int argc, char *args[]);

char *get_mime_type(char *name);

void handleRequest(int sd, char *method, char *path, char *protocol, char *response);

void createResponse(void *SD);

void requestParse(int argc, char *argv[]);

int socketCreation(int, int maxRequests);

int isFile(char *path) {
    struct stat path_stat;
    stat(path, &path_stat);
    if (stat(path, &path_stat) != 0) {
        // Handle error
        return -1;
    }
    if (S_ISREG(path_stat.st_mode)) {
        //path is a file
        return 1;
    }
    else if (S_ISDIR(path_stat.st_mode)) {
        // path is a directory
        return 2;
    }
    else {
        // path is something else
        return 3;
    }
}

/*Utility function to get type of file in requested path*/
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

int checkValid(int argc, char *args[]) {
    if (argc != 4)
        return 0;
    for (int i = 1; i < 4; i++) {
        for (int j = 0; j < strlen(args[i]); j++) {
            if (!('0' <= args[i][j] && args[i][j] <= '9'))
                return 0;
        }
    }
    return 1;
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
    if (listen(fd, maxRequests) < 0) {
        perror("listen");
        exit(1);
    }
    return fd;
}

int main(int argc, char *argv[]) {
    requestParse(argc, argv);
}

void requestParse(int argc, char *argv[]) {
    if (checkValid(argc, argv) == 0) {
        fprintf(stderr, "Usage: <port> <pool-size> <max-number-of-request>\n");
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

void handleRequest(int sd, char *method, char *path, char *protocol, char *response) {
    int exists = access(path, F_OK);
    int is_file = isFile(path);
    char date[128];
    getDate(date);
    if (strchr(protocol, ' ') != NULL) {
        sprintf(response, "HTTP/1.0 400 Bad Request\r\n"
                          "Server: webserver/1.0\r\n"
                          "Date: %s\r\n"
                          "Content-Type: text/html\r\n"
                          "Content-Length: 113\r\n"
                          "Connection: close\r\n"
                          "\r\n"
                          "<HTML><HEAD><TITLE>400 Bad Request</TITLE></HEAD>\n"
                          "<BODY><H4>400 Bad request</H4>\n"
                          "Bad Request.\n"
                          "</BODY></HTML>\n", date);
        write(sd, response, strlen(response));
    } else if (strstr(method, "GET") == NULL) {//send 501 not supported (File 501.txt)
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
        write(sd, response, strlen(response));
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
        write(sd, response, strlen(response));
    } else if (is_file != 1 && path[strlen(path) - 1] != '/' && strcmp(path, "/") != 0) { // 302 Error
        sprintf(response, "HTTP/1.0 302 Found\r\n"
                          "Server: webserver/1.0\r\n"
                          "Date: %s\r\n"
                          "Location: %s/\r\n"
                          "Content-Type: text/html\r\n"
                          "Content-Length: 123\r\n"
                          "Connection: close\r\n"
                          "\r\n"
                          "<HTML><HEAD><TITLE>302 Found</TITLE></HEAD>\n"
                          "<BODY><H4>302 Found</H4>\n"
                          "Directories must end with a slash.\n"
                          "</BODY></HTML>\n", date, path);
        write(sd, response, strlen(response));
    } else if (is_file != 1 && path[strlen(path) - 1] == '/') { // Search for index.html in dir
        char indexPath[strlen(path) + strlen("index.html") + 1];
        strcpy(indexPath, path);
        strcat(indexPath, "index.html");
        struct stat statsOfFile;
        if (access(indexPath, F_OK) != -1) {
            stat(indexPath, &statsOfFile);
            int fd = open(indexPath, O_RDONLY);
            char *mime = get_mime_type(indexPath);
            char reader[BYTE] = {0};
            size_t bytesRead = 0;
            size_t cur;
            sprintf(response, "HTTP/1.0 200 OK\r\n"
                              "Server: webserver/1.0\r\n"
                              "Date: %s\r\n"
                              "Content-Type: %s\r\n"
                              "Content-Length: %zu\r\n"
                              "Connection: close\r\n\r\n", date, mime, statsOfFile.st_size);
            write(sd, response, strlen(response));
            while ((cur = read(fd, reader, BYTE)) > 0) {
                write(sd, reader, strlen(reader));
                bytesRead += cur;
            }
        } else {
            sprintf(response, "HTTP/1.1 200 OK\r\n"
                              "Server: webserver/1.0\r\n"
                              "Date: %s\r\n"
                              "Content-Type: text/html\r\n"
                              "Content-Length: <content-length>\r\n"
                              "Last-Modified: %s\r\n"
                              "Connection: close\r\n\r\n", date, date);
            write(sd, response, strlen(response));
            sprintf(response, "<HTML>\n"
                              "<HEAD><TITLE>Index of %s</TITLE></HEAD>\n"
                              "<BODY>\n"
                              "<H4>Index of %s</H4>\n"
                              "<table CELLSPACING=8>\n"
                              "<tr><th>Name</th><th>Last Modified</th><th>Size</th></tr>\n", path+1, path+1);
            write(sd,response, strlen(response));
            DIR *d = opendir(path);
            struct dirent *dir;
            struct stat sb;
            strcpy(response,"");
            if (d) {
                while ((dir = readdir(d)) != NULL) {
                    char curFileName[200];
                    char lastModified[128];
                    strcpy(response,"<tr>");
                    strcat(response,"<td>");
                    strcat(response,"<a href = \"");
                    strcpy(curFileName,path);
                    strcat(curFileName, dir->d_name);
                    int type = isFile(curFileName);
                    stat(curFileName,&sb);
                    strcpy(curFileName,dir->d_name);
                    if (type == 2){
                        strcat(curFileName, "/");
                    }
                    strcat(response, curFileName);
                    strcat(response, "\">");
                    strcat(response,dir->d_name);
                    strcat(response,"</a></td>");
                    strcat(response,"<td>");
                    strftime(lastModified, 128, RFC1123FMT, gmtime(&sb.st_mtime));
                    strcat(response,lastModified);
                    strcat(response,"</td>");
                    if(type == 1){
                        char fileSize[100];
                        sprintf(fileSize, "<td> %ld </td>\n", sb.st_size);
                        strcat(response, fileSize);
                    }
                    strcat(response,"</tr>");
                    write(sd,response,strlen(response));
                }
                closedir(d);
            }

            strcpy(response, "</table>\n<HR>\n<ADDRESS>webserver/1.0</ADDRESS>\n</BODY></HTML>");
            write(sd,response,strlen(response));
        }
    } else if (is_file == 1) { //Path is a file
        char *getMime = get_mime_type(path);
        if (getMime == NULL) { //If file type is not specified.

        }
        if (access(path, R_OK) != -1) {

        }
    }
}

void createResponse(void *SD) {
    int socketDescriptor = *((int *) SD);
    char buffer[BYTE] = {0};
    char *response = alloc(char, BYTE * 10);
    memset(response, '\0', BYTE);
    char *end = NULL;
    size_t bytes_read, total_bytes_read = 0;
    while ((bytes_read = read(socketDescriptor, buffer, BYTE)) > 0) {
        total_bytes_read += bytes_read;
        if ((end = strstr(buffer, "\r\n")))
            break;
    }
    if (end)
        *end = '\0';
    /*Split the first line in the request to the method, path, and protocol by using strtok and using space as delimiter*/
    char *method = strtok(buffer, " ");
    char *path = strtok(NULL, " ");
    char curPath[strlen(path) + 2]; // add . to path to serve as root directory
    strcpy(curPath, ".");
    strcat(curPath, path);
    path = curPath;
    char *protocol = strtok(NULL, "\0");
    handleRequest(socketDescriptor, method, path, protocol, response);
    free(response);
    close(socketDescriptor);
}