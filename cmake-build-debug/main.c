#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <malloc.h>
#include <dirent.h>
#include <sys/stat.h>
#include <fcntl.h>
//#include "server.c"
//#include "threadpool.h"



int main(int argc, char *argv[]){
    char *path = "./Testing";
    DIR *d = opendir(path);
    struct dirent *dir;
    if (d) {
        while ((dir = readdir(d)) != NULL) {
            printf("%s\n", dir->d_name);
        }
        closedir(d);
    }
//    struct stat sb1;
//    stat(path,&sb1);
//    printf("size of = %ld\n",sb1.st_size);

    return 0;
}
