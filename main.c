#include <stdio.h>
#include "server.c"
#include "threadpool.h"

int main(int argc, char *argv[]){
    char date[128];
    getDate(date);
    printf("%s\n",date);
    return 0;
}
