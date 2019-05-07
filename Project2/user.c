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

int srv_fifo_fd;
int user_fifo_fd;
char *user_fifo;

int user_id;
char *pass = "";
int delay;
int op;
char *arg = "";

op_type_t op_type;

req_header_t req_header;

req_create_account_t req_create_account;
req_transfer_t req_transfer;


void openServerFIFO(){
    if(mkfifo(SERVER_FIFO_PATH, FIFO_RW_MODE) != 0){
        printf("%s nao foi possivel abrir\n", SERVER_FIFO_PATH);
        exit(1);
    }

    srv_fifo_fd = open(SERVER_FIFO_PATH, O_WRONLY);
}

void openUserFIFO(){
    if(mkfifo(user_fifo, FIFO_RW_MODE) != 0){
        printf("%s nao foi possivel abrir\n", user_fifo);
        exit(2);
    }

    user_fifo_fd = open(user_fifo, O_RDONLY);
}

void getUserFIFOName(){
    char *pid_buf = malloc(sizeof(WIDTH_ID));

    pid_t pid = getpid();

    sprintf(pid_buf, "%*d",WIDTH_ID,pid);

    user_fifo = malloc(USER_FIFO_PATH_LEN);

    strcpy(user_fifo, USER_FIFO_PATH_PREFIX);
    strcat(user_fifo, pid_buf);
}

void adminAcess(){
    write(srv_fifo_fd, &user_id,sizeof(int));
    write(srv_fifo_fd, pass, sizeof(char *) * strlen(pass));
    if(op == 0){
        write(srv_fifo_fd, arg, sizeof(char *) * strlen(arg));
    }
    else if(op == 3){
    }
}

void userAcess(){

}

int main(int argc, char *argv[]){
    if(argc < 5 || argc > 5){
        printf("%s precisa de 5 argumentos\n", argv[0]);
        exit(1);
    }

    mode_t old_mask = umask(NEW_MASK);

    user_id = atoi(argv[1]);
    if(user_id > MAX_BANK_ACCOUNTS){
        printf("Numero maximo de contas ultrapassado, limite: %d", MAX_BANK_ACCOUNTS);
        exit(2);
    }
    pass = argv[2];
    if(strlen(pass) < MIN_PASSWORD_LEN || strlen(pass) > MAX_PASSWORD_LEN){
        printf("Insira uma password entre %s e %s\n", MIN_PASSWORD_LEN, MAX_PASSWORD_LEN);
        exit(3);
    }
    delay = atoi(argv[3]);
    if(delay > MAX_OP_DELAY_MS){
        printf("Atraso de operacao deve ser inferior a %d", MAX_OP_DELAY_MS);
        exit(4);
    }
    op_type = atoi(argv[4]);
    if(op_type < 0 || op_type > 3){
        printf("Codigo de operacao entre 0 e 3\n");
        exit(5);
    }
    arg = argv[5];

    getUserFIFOName();


    if(user_id == ADMIN_ACCOUNT_ID){
        adminAcess();
    }
    else {
        userAcess();
    }




    umask(old_mask);

    return 0;
}