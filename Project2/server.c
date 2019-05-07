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

bank_account_t bank_accounts[MAX_BANK_ACCOUNTS];

int srv_fifo_fd;

int num_threads;
char *admin_pass = "";

void openServerFIFO(){
    if(mkfifo(SERVER_FIFO_PATH, FIFO_RW_MODE) != 0){
        printf("%s nao foi possivel abrir\n", SERVER_FIFO_PATH);
        exit(1);
    }

    srv_fifo_fd = open(SERVER_FIFO_PATH, O_RDONLY);
}

void *balcaoEletronico(void *arg){

}


int main(int argc, char *argv[]){
    if(argc > 3){
        printf("%s precisa de 2 argumentos", argv[0]);
        exit(1);

    }
    mode_t old_mask = umask(NEW_MASK);

    num_threads = atoi(argv[1]);
    if(num_threads > MAX_BANK_OFFICES){
        printf("O maximo permitido e' %d\n", MAX_BANK_OFFICES);
        exit(2);
    }
    admin_pass = argv[2];
    if(strlen(admin_pass) < MIN_PASSWORD_LEN || strlen(admin_pass) > MAX_PASSWORD_LEN){
        printf("Insira uma password entre %d e %d caracteres\n", MIN_PASSWORD_LEN, MAX_PASSWORD_LEN);
        exit(3);
    }

    pthread_t thread [num_threads];

    for(int i = 0; i < num_threads; i++){
        if(pthread_create(&thread[i],NULL,balcaoEletronico, (void *) &bank_accounts[i]) != 0){
            exit(4);
        }
    }

    umask(old_mask);

    return 0;
}