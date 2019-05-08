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

pthread_mutex_t mut = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t cond_var = PTHREAD_COND_INITIALIZER;

bank_account_t bank_accounts[MAX_BANK_ACCOUNTS];
bank_account_t bank_account;



int srv_fifo_fd;

int num_threads;
char *admin_pass = "";

static const char caracteres[] = "0123456789abcdef";

char *salt = "";

void generateSalt(){
    int i;
    for(i = 0; i  < SALT_LEN; i++){
        int n = rand() % (int) (sizeof caracteres - 1);
        salt[i] = caracteres[n];
    }
    salt[i] = '\0';
}

char *getHash(char *pass){
    FILE *fp;

    strcat(pass, salt);

    char *command;
    char hash[HASH_LEN];

    sprintf(command, "echo -n %s | sha256sum",pass); 

    fp = popen(command,"r");

    fgets(hash, HASH_LEN, fp);

    pclose(fp);

    return hash;
}

void openServerFIFO(){
    if(mkfifo(SERVER_FIFO_PATH, FIFO_RW_MODE) != 0){
        printf("%s nao foi possivel abrir\n", SERVER_FIFO_PATH);
        exit(1);
    }

    srv_fifo_fd = open(SERVER_FIFO_PATH, O_RDONLY);
}

sem_t empty, full;

void *balcaoEletronico(void *conta){

}

void *pedidos(void *arg){
    int pedidos;


}

void autenticacao(){

}


int main(int argc, char *argv[]){
    if(argc != 3){
        printf("%s precisa de 2 argumentos", argv[0]);
        exit(1);

    }
    srand(time(NULL));
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
        if(pthread_create(&thread[i],NULL,balcaoEletronico, (void *) &bank_account) != 0){
            perror("Nao foi possivel criar thread\n");
            exit(4);
        }
    }

    for(int i = 0; i < num_threads; i++){
        pthread_join(thread[i],NULL);
    }

    umask(old_mask);

    return 0;
}