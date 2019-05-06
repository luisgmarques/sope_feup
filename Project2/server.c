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
    int num_threads = atoi(argv[1]);
    if(num_threads > MAX_BANK_OFFICES){
        printf("Numero maximo de balcões e' %d\n", MAX_BANK_OFFICES);
        exit(1);
    }
    mkfifo(SERVER_FIFO_PATH, 0777);

    fifo_fd = open(SERVER_FIFO_PATH, O_RDONLY);

    pthread_t thread [num_threads];

    for(int i = 0; i < num_threads; i++){
        if(pthread_create(&thread[i],NULL,,) != 0){

        }
        logBankOfficeOpen(STDOUT_FILENO,MAIN_THREAD_ID,)
    }

    return 0;
}