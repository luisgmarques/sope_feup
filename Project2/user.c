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
#include <errno.h>

#include "sope.h"

int srv_fifo_fd = -1;
int user_fifo_fd;
char *user_fifo = "";

int user_id;
char pass[MAX_PASSWORD_LEN + 1];
int delay;
char *arg = "";

uint32_t length = 0;

op_type_t op_type;

req_header_t req_header;

req_create_account_t req_create_account;
req_transfer_t req_transfer;

req_value_t req_value;

tlv_request_t tlv_request;


void openServerFIFO(){
    do{
        srv_fifo_fd = open(SERVER_FIFO_PATH, O_WRONLY);
        //printf("%d", srv_fifo_fd);
        if(srv_fifo_fd == -1){
            sleep(1);
        }
    }while(srv_fifo_fd == -1);  
}

void makeUserFIFO(){
    if(mkfifo(user_fifo, FIFO_RW_MODE) != 0){
        printf("%s nao foi possivel abrir\n", user_fifo);
        exit(2);
    }
}

void openUserFIFO(){
    
    do{
        user_fifo_fd = open(user_fifo, O_RDONLY | O_NONBLOCK);
        if(user_fifo_fd == -1){
            sleep(1);
        }
    }while(user_fifo_fd == -1);
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
    if(op_type == OP_SHUTDOWN){
        return;
    }
    else if(op_type == OP_CREATE_ACCOUNT){
        char *token;
        token = strtok(arg, " ");
        req_create_account.account_id = atoi(token);
        token = strtok(NULL, " ");
        req_create_account.balance = atoi(token);
        token = strtok(NULL, " ");
        strcpy(req_create_account.password, token);

        req_value.create = req_create_account;

        length += sizeof(req_create_account.account_id) + sizeof(req_create_account.balance) + strlen(req_create_account.password);

        return; 
    }
}

void userAcess(){
    if(op_type == OP_BALANCE){
        return;
    }
    else if(op_type == OP_TRANSFER){
        char *token;
        token = strtok(arg, " ");
        req_transfer.account_id = atoi(token);
        token = strtok(NULL, " ");
        req_transfer.amount = atoi(token);

        req_value.transfer = req_transfer;

        length += sizeof(req_transfer.account_id) + sizeof(req_transfer.amount);

        return;
    }
}

void fillReqStruct(){
    req_header.account_id = user_id;
    req_header.op_delay_ms = delay;
    strcpy(req_header.password, pass);
    req_header.pid = getpid();

    length += sizeof(user_id) + sizeof(delay) + strlen(pass) + sizeof(req_header.pid);
}

void fillValueStruct(){
    req_value.header = req_header;
    
}

void fillTLVStruct(){
    tlv_request.type = op_type;
    tlv_request.value = req_value;
    tlv_request.length = length;
    
}

void processArgs(char *args[]){
    user_id = atoi(args[1]);
    if(user_id > MAX_BANK_ACCOUNTS){
        printf("Numero maximo de contas ultrapassado, limite: %d", MAX_BANK_ACCOUNTS);
        exit(2);
    }
    strcpy(pass, args[2]);
    if(strlen(pass) < MIN_PASSWORD_LEN || strlen(pass) > MAX_PASSWORD_LEN){
        printf("Insira uma password entre %d e %d\n", MIN_PASSWORD_LEN, MAX_PASSWORD_LEN);
        exit(3);
    }
    delay = atoi(args[3]);
    if(delay > MAX_OP_DELAY_MS){
        printf("Atraso de operacao deve ser inferior a %d", MAX_OP_DELAY_MS);
        exit(4);
    }
    op_type = atoi(args[4]);
    if(op_type < 0 || op_type > 3){
        printf("Codigo de operacao entre 0 e 3\n");
        exit(5);
    }
    arg = args[5];
}

int main(int argc, char *argv[]){
    if(argc != 6){
        printf("%s precisa de 5 argumentos\n", argv[0]);
        printf("Usage: %s <user_id> <user_pass> <op_delay> <op_type> <args_op>\n", argv[0]);
        exit(1);
    }

    mode_t old_mask = umask(NEW_MASK);

    processArgs(argv);

    getUserFIFOName();

    makeUserFIFO();

    if(user_id == ADMIN_ACCOUNT_ID){
        adminAcess();
    }
    else {
        userAcess();
    }

    fillReqStruct();

    fillValueStruct();

    fillTLVStruct();

    openServerFIFO();

    logRequest(STDOUT_FILENO, getpid(), &tlv_request);

    write(srv_fifo_fd, &tlv_request, sizeof(tlv_request_t));
    
    openUserFIFO();
    
    tlv_reply_t *reply = (tlv_reply_t *)malloc(sizeof(tlv_reply_t));
    
    int counter = 0;

    while(counter < FIFO_TIMEOUT_SECS){
        
        if(read(user_fifo_fd, reply, sizeof(tlv_reply_t)) > 0){
            break;
        }
        sleep(1);
        counter++;
    }

    if(counter >= FIFO_TIMEOUT_SECS){

        close(user_fifo_fd);

        close(srv_fifo_fd);

        unlink(user_fifo);

        umask(old_mask);

        return 1;
    }
    
    logReply(STDOUT_FILENO, getpid(), reply);
    
    close(user_fifo_fd);

    close(srv_fifo_fd);

    unlink(user_fifo);

    umask(old_mask);

    return 0;
}