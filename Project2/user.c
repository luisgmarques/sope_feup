#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <time.h>
#include <sys/time.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/file.h>
#include <string.h>
#include <pthread.h>
#include <time.h>
#include <semaphore.h>

#include "sope.h"
#include "types.h"

int fifo_fd;

int main(int argc, char *argv[]){
    umask(0000);
    mkfifo(SERVER_FIFO_PATH, 0777);

    fifo_fd = open(SERVER_FIFO_PATH, O_WRONLY);

    mkfifo()

    return 0;
}