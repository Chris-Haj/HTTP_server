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
void InternalError(int socket, char *date) {
    char error[500];
    sprintf(error, "HTTP/1.1 500 Internal Server Error\r\n"
                   "Server: webserver/1.0\r\n"
                   "Date: %s\r\n"
                   "Content-Type: text/html\r\n"
                   "Content-Length: 144\r\n"
                   "Connection: close\r\n"
                   "\r\n"
                   "<HTML><HEAD><TITLE>500 Internal Server Error</TITLE></HEAD>\n"
                   "<BODY><H4>500 Internal Server Error</H4>\n"
                   "Some server side error.\n"
                   "</BODY></HTML>", date);
    write(socket, error, strlen(error));
}

int checkPermsForPath(char *path) {
    char *copy = alloc(char, strlen(path) + 1);
    strcpy(copy, path);
    char *beginning = copy;
    copy = strchr(copy, '/');
    if (copy == NULL) {
        free(beginning);
        return 1;
    }
    struct stat permsFiles;
    while (*copy) { // while copy has not reached the end
        if (*copy == '/') {
            *copy = '\0';
            //if stat fails call InternalError
            if (stat(beginning, &permsFiles) == -1) {
                free(beginning);
                return -1;
            }
            if ((permsFiles.st_mode & (S_IRUSR | S_IRGRP | S_IROTH)) != (S_IRUSR | S_IRGRP | S_IROTH)) {
                *copy = '/';
                free(beginning);
                return 0;
            }
            *copy = '/';
        }
        copy++;
    }
    free(beginning);
    return 1;
}

int checkValid(int argc, char *args[]);

int isFile(char *path) {
    struct stat path_stat;
    stat(path, &path_stat);
    if (stat(path, &path_stat) == -1) {
        return -1;
    }
    if (S_ISREG(path_stat.st_mode)) {
        //path is a file
        return 1;
    } else if (S_ISDIR(path_stat.st_mode)) {
        // path is a directory
        return 2;
    } else {
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

/*Checks if all 4 arguments were entered into the main function and also checks
 * if all the numbers entered are valid*/
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

/*This functions creates a socket and inits all its parameters
 * the socket is opened as a server side socket
 * we bind it, and we make it listen to the max number of requests*/
int socketCreation(int port, int maxRequests) {
    struct sockaddr_in sockIn = {0};
    sockIn.sin_family = AF_INET;
    sockIn.sin_addr.s_addr = htonl(INADDR_ANY);
    sockIn.sin_port = htons(port);
    int fd;
    if ((fd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("socket");
        exit(0);
    }
    if (bind(fd, (struct sockaddr *) &sockIn, sizeof(sockIn)) < 0) {
        perror("bind");
        exit(0);
    }
    if (listen(fd, maxRequests) < 0) {
        perror("listen");
        exit(0);
    }
    return fd;
}

void handleRequest(int sd, char *method, char *path, char *protocol);

void createResponse(void *SD);

void requestParse(int argc, char *argv[]);

int main(int argc, char *argv[]) {
    requestParse(argc, argv);
}

/*makes sure the command usage is correct.
 * creates threadpool, and after opening a main socket, it uses the accept function receive another socket connected to
 * the main one and sends it to a thread's work function to handle its request;*/
void requestParse(int argc, char *argv[]) {
    if (checkValid(argc, argv) == 0) {
        printf("Usage: <port> <pool-size> <max-number-of-request>\n");
        exit(0);
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
            destroy_threadpool(pool);
            exit(0);
        }
        //if dispatch fails free threadpool then exit
        dispatch(pool, (void *) createResponse, (void *) &serverSockets);
        i++;
    }
    close(socketFD);
    destroy_threadpool(pool);
}

void handleRequest(int sd, char *method, char *path, char *protocol) {
    char response[BYTE * 10];
    int exists = access(path, F_OK);
    int is_file = isFile(path);
    char date[128];
    getDate(date);
    int perms = checkPermsForPath(path);
    //If one of the arguments was still empty or the protocol method was unknown send a bad request error
    if (method[0] == '\0' || path[0] == '\0' || protocol[0] == '\0' || (strncmp(protocol, "HTTP/", 5) != 0)) {
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
        //if write fails call InternalServerError
        if (write(sd, response, strlen(response)) < 0) {
            InternalError(sd, date);
        }
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
        if (write(sd, response, strlen(response)) < 0) {
            InternalError(sd, date);
        }
    } else if (perms == 0) {//send 403 forbidden (File 403.txt)){
        sprintf(response, "HTTP/1.1 403 Forbidden\r\n"
                          "Server: webserver/1.0\r\n"
                          "Date: %s\r\n"
                          "Content-Type: text/html\r\n"
                          "Content-Length: 111\r\n"
                          "Connection: close\r\n"
                          "\r\n"
                          "<HTML><HEAD><TITLE>403 Forbidden</TITLE></HEAD>\n"
                          "<BODY><H4>403 Forbidden</H4>\n"
                          "Access denied.\n"
                          "</BODY></HTML>", date);
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
        if (write(sd, response, strlen(response)) < 0) {
            InternalError(sd, date);
        }
    } else if (is_file == 2 && path[strlen(path) - 1] != '/' && strcmp(path, "/") != 0) { // 302 Error
        sprintf(response, "HTTP/1.0 302 Found\r\n"
                          "Server: webserver/1.0\r\n"
                          "Date: %s\r\n"
                          "Location: %s/ \r\n"
                          "Content-Type: text/html\r\n"
                          "Content-Length: 123\r\n"
                          "Connection: close\r\n"
                          "\r\n"
                          "<HTML><HEAD><TITLE>302 Found</TITLE></HEAD>\n"
                          "<BODY><H4>302 Found</H4>\n"
                          "Directories must end with a slash.\n"
                          "</BODY></HTML>\n", date, path);
        if (write(sd, response, strlen(response)) < 0) {
            InternalError(sd, date);
        }
    } else if (is_file != 1 && path[strlen(path) - 1] == '/') { // Search for index.html in dir
        char indexPath[strlen(path) + strlen("index.html") + 1];
        strcpy(indexPath, path);
        strcat(indexPath, "index.html");
        struct stat statsOfFile;
        if (access(indexPath, F_OK) != -1) {
            if(stat(indexPath, &statsOfFile) == -1){
                InternalError(sd,date);
                return;
            }
            int fd = open(indexPath, O_RDONLY);
            if (fd < 0){
                InternalError(sd,date);
                return;
            }
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
            if (write(sd, response, strlen(response)) < 0) {
                InternalError(sd, date);
            }
            while ((cur = read(fd, reader, BYTE)) > 0) {
                write(sd, reader, cur);
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
            if (write(sd, response, strlen(response)) < 0) {
                InternalError(sd, date);
            }
            sprintf(response, "<HTML>\n"
                              "<HEAD><TITLE>Index of %s</TITLE></HEAD>\n"
                              "<BODY>\n"
                              "<H4>Index of %s</H4>\n"
                              "<table CELLSPACING=8>\n"
                              "<tr><th>Name</th><th>Last Modified</th><th>Size</th></tr>\n", path + 1, path + 1);
            if (write(sd, response, strlen(response)) < 0) {
                InternalError(sd, date);
            }
            DIR *d = opendir(path);
            struct dirent *dir;
            struct stat sb;
            strcpy(response, "");
            if (d) {
                while ((dir = readdir(d)) != NULL) {
                    char curFileName[200];
                    char lastModified[128];
                    strcpy(response, "<tr>");
                    strcat(response, "<td>");
                    strcat(response, "<a href = \"");
                    strcpy(curFileName, path);
                    strcat(curFileName, dir->d_name);
                    int type = isFile(curFileName);
                    stat(curFileName, &sb);
                    strcpy(curFileName, dir->d_name);
                    if (type == 2) {
                        strcat(curFileName, "/");
                    }
                    strcat(response, curFileName);
                    strcat(response, "\">");
                    strcat(response, dir->d_name);
                    strcat(response, "</a></td>");
                    strcat(response, "<td>");
                    strftime(lastModified, 128, RFC1123FMT, gmtime(&sb.st_mtime));
                    strcat(response, lastModified);
                    strcat(response, "</td>");
                    if (type == 1) {
                        char fileSize[100];
                        sprintf(fileSize, "<td> %ld </td>\n", sb.st_size);
                        strcat(response, fileSize);
                    }
                    strcat(response, "</tr>");
                    write(sd, response, strlen(response));
                }
                closedir(d);
            }
            strcpy(response, "</table>\n<HR>\n<ADDRESS>webserver/1.0</ADDRESS>\n</BODY></HTML>");
            if (write(sd, response, strlen(response)) < 0) {
                InternalError(sd, date);
            }
        }
    } else if (is_file == 1) { //Path is a file
        bzero(response, BYTE * 10);
        char *getMime = get_mime_type(path);
        struct stat sb;
        stat(path, &sb);
        long int contentLength = sb.st_size;
        char lastModified[128];
        strftime(lastModified, 128, RFC1123FMT, gmtime(&sb.st_ctime));
        if (getMime == NULL) { //If file type is not specified.
            sprintf(response, "HTTP/1.0 200 OK\r\n"
                              "Server: webserver/1.0\r\n"
                              "Date: %s\r\n"
                              "Content-Type: %s\r\n"
                              "Content-Length: %ld\r\n"
                              "Last-Modified: %s\r\n"
                              "Connection: close\r\n\r\n", date, "", contentLength, lastModified);
        } else {
            sprintf(response, "HTTP/1.0 200 OK\r\n"
                              "Server: webserver/1.0\r\n"
                              "Date: %s\r\n"
                              "Content-Type: %s\r\n"
                              "Content-Length: %ld\r\n"
                              "Last-Modified: %s\r\n"
                              "Connection: close\r\n\r\n", date, getMime, contentLength, lastModified);
        }
        if (write(sd, response, strlen(response)) < 0) {
            InternalError(sd, date);
        }
        unsigned char buffer[BYTE + 1];
        buffer[BYTE] = '\0';
        if (access(path, R_OK) != -1) {
            int fd = open(path, O_RDONLY);
            size_t bytes;
            while ((bytes = read(fd, buffer, BYTE - 1)) > 0) {
                write(sd, buffer, bytes);
            }
        }
    }
    close(sd);
}

void createResponse(void *SD) {
    int socketDescriptor = *((int *) SD);
    char buffer[BYTE] = {0};
    char *end = NULL;
    size_t bytes_read, total_bytes_read = 0;
    while ((bytes_read = read(socketDescriptor, buffer, BYTE)) > 0) {
        if (bytes_read == -1) {
            perror("read");
            exit(0);
        }
        total_bytes_read += bytes_read;
        if ((end = strstr(buffer, "\r\n")))
            break;
    }
    if (end) //After finding the first \r\n, we cut off the rest of the buffer by replacing it with a null terminator.
        *end = '\0';
    char method[10] = {0}, path[200] = {0}, protocol[10] = {0};
    sscanf(buffer, "%s %s %s", method, path, protocol);
    char curPath[strlen(path) + 2]; // add . to path to serve as root directory
    strcpy(curPath, ".");
    strcat(curPath, path);
    handleRequest(socketDescriptor, method, curPath, protocol);
    close(socketDescriptor);
}