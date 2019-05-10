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

pthread_mutex_t mut = PTHREAD_MUTEX_INITIALIZER; // Mutex
sem_t sem1, sem2; // Semaforo
int val1, val2; // valor do semaforo

bank_account_t bank_accounts[MAX_BANK_ACCOUNTS]; // Guarda as contas dos users
int num_accounts = 0; //numero de contas no banco

int srv_fifo_fd; // Descritor do fifo srv
int user_fifo_fd;

char *user_fifo = "";

tlv_request_t buffer; // Buffer usado para comunicao entre Produtor e Consumidor

int num_threads; // Numero de banco eletronicos

static const char caracteres[] = "0123456789abcdef";

char *getSalt(){
    char *salt = (char *) malloc(sizeof(char) * (SALT_LEN + 1));
    int i;
    for(i = 0; i  < SALT_LEN; i++){
        int n = rand() % (int) (sizeof caracteres - 1);
        salt[i] = caracteres[n];
    }
    salt[i] = '\0';

    return salt;
}

char *getHash(char *pass, char *salt){
    FILE *fp;

    strcat(pass, salt);

    char command[256];
    char *hash  = (char *) malloc(sizeof(char) * (HASH_LEN + 1));

    sprintf(command, "echo -n %s | sha256sum",pass); 

    fp = popen(command,"r");

    if(fgets(hash, HASH_LEN + 1, fp) != NULL){
        pclose(fp);

        return hash;
    }

    return NULL;
}

void openServerFIFO(){
    if(mkfifo(SERVER_FIFO_PATH, FIFO_RW_MODE) != 0){
        if(errno == EEXIST){
            printf("Server FIFO - %s - ja existe\n", SERVER_FIFO_PATH);
        }
        else
            printf("%s nao foi possivel criar FIFO\n", SERVER_FIFO_PATH);
        unlink(SERVER_FIFO_PATH);
        exit(1);
    }

    srv_fifo_fd = open(SERVER_FIFO_PATH, O_RDONLY);
}

void openUserFIFO(pid_t pid){
    char *buf = malloc(sizeof(WIDTH_ID + 1));
    sprintf(buf, "%*d", WIDTH_ID, pid);

    user_fifo = malloc(USER_FIFO_PATH_LEN);

    strcpy(user_fifo, USER_FIFO_PATH_PREFIX);
    strcat(user_fifo, buf);

    user_fifo_fd = open(user_fifo, O_WRONLY);
}

int login(uint32_t id, char *pass){
    for(int i = 0; i < num_accounts; i++){
        if(bank_accounts[i].account_id == id){
            char *hash = getHash(pass, bank_accounts[i].salt);
            if(strcmp(bank_accounts[i].hash, hash) != 0){
                return 0;
            }
            return 1;
        }
    }

    return 0;
}

void processArgs(char *args[]){
    num_threads = atoi(args[1]);
    if(num_threads > MAX_BANK_OFFICES){
        printf("O maximo permitido e' %d\n", MAX_BANK_OFFICES);
        exit(2);
    }
    if(strlen(args[2]) < MIN_PASSWORD_LEN || strlen(args[2]) > MAX_PASSWORD_LEN){
        printf("Insira uma password entre %d e %d caracteres\n", MIN_PASSWORD_LEN, MAX_PASSWORD_LEN);
        exit(3);
    }
}

int createAccount(uint32_t account_id, uint32_t balance, char *pass){
    bank_account_t *bank_account = malloc(sizeof(bank_account_t));

    for(int i = 0; i < num_accounts; i++){
        if(bank_accounts[i].account_id == account_id){
            return RC_ID_IN_USE;
        }
    }
    
    bank_account->account_id = ADMIN_ACCOUNT_ID;

    bank_account->balance = balance;

    char *salt = getSalt();
    strcpy(bank_account->salt, salt);

    char *hash = getHash(pass, salt);
    strcpy(bank_account->hash, hash); 

    bank_accounts[num_accounts] = *bank_account;
    num_accounts++;

    return RC_OK;
}

int transfer(uint32_t source_id, uint32_t balance, uint32_t dest_id){
    int i;
    for(i = 0; i < num_accounts; i++){
        if(bank_accounts[i].account_id == dest_id){
            break;
        }
    }

    if(i == num_accounts){
        return RC_ID_NOT_FOUND;
    }

    if(source_id == dest_id){
        return RC_SAME_ID;
    }

    bank_account_t source_account, dest_account;
    for(int k = 0; k < num_accounts; k++){
        if(bank_accounts[k].account_id == source_id){
            source_account = bank_accounts[k];
        }
        else if(bank_accounts[k].account_id == dest_id){
            dest_account = bank_accounts[k];
        }
    }

    if(source_account.balance < balance){
        return RC_NO_FUNDS;
    }

    if(dest_account.balance + balance > MAX_BALANCE){
        return RC_TOO_HIGH;
    }

    return RC_OK;
}

int checkBalance(uint32_t account_id){
    bank_account_t account;
    for(int i = 0; i < num_accounts; i++){
        if(bank_accounts[i].account_id == account_id){
            account = bank_accounts[i];
            break;
        }
    }

    return account.balance;
}

void shutdown(){
    fchmod(srv_fifo_fd, FIFO_READ_ONLY);
}

void *balcaoEletronico(void *num){
    logBankOfficeOpen(STDOUT_FILENO, *(int *) num, pthread_self());
    tlv_reply_t tlv_reply;
    while(1){
        sem_wait(&sem2);
        sem_getvalue(&sem2, &val1);
        logSyncMechSem(STDOUT_FILENO, *(int *) num, SYNC_OP_SEM_WAIT, SYNC_ROLE_CONSUMER, 0, val1);
        
        pthread_mutex_lock(&mut);
        usleep(buffer.value.header.op_delay_ms * 1000);
        if(login(buffer.value.header.account_id, buffer.value.header.password)){
            switch(buffer.type){

                /**     CREATE ACCOUNT      **/

                case(OP_CREATE_ACCOUNT):
                if(buffer.value.header.account_id == 0){
                    int n = createAccount(buffer.value.create.account_id, buffer.value.create.balance, buffer.value.create.password);
                    tlv_reply.value.header.ret_code = n;
                    logAccountCreation(STDOUT_FILENO, *(int *) num, &bank_accounts[num_accounts-1]);
                }
                else if(buffer.value.header.account_id > 0){
                    tlv_reply.value.header.ret_code = RC_OP_NALLOW;
                }

                else{
                    tlv_reply.value.header.ret_code = RC_OTHER;
                }

                tlv_reply.type = buffer.type;
                tlv_reply.value.header.account_id = *(int *)num;
                

                break;


                /**     CHECK BALANCE       **/

                case(OP_BALANCE):
                if(buffer.value.header.account_id > 0){
                    pthread_mutex_lock(&mut);
                    int i;
                    for(i = 0; i < MAX_BANK_ACCOUNTS; i++){
                        if(bank_accounts[i].account_id == buffer.value.header.account_id){
                            break;
                        }
                    }

                    if(i < MAX_BANK_ACCOUNTS){
                        tlv_reply.value.balance.balance = bank_accounts[i].balance;
                    }
                    else{

                    }

                }
                else{

                }

                /**     SERVER SHUTDOWN     **/

                case(OP_SHUTDOWN):

                /**     TRANSFERENCIA       **/

                case(OP_TRANSFER):

                default:
                break;
            }
        }
        else{
            tlv_reply.type = buffer.type;
            tlv_reply.value.header.account_id = *(int *)num;
            tlv_reply.value.header.ret_code = RC_LOGIN_FAIL;
        }
        
        pthread_mutex_unlock(&mut);
        logReply(STDOUT_FILENO, *(int *)num, &tlv_reply);
        openUserFIFO(buffer.value.header.pid);

        write(user_fifo_fd, &tlv_reply, sizeof(tlv_reply_t));
        close(user_fifo_fd);
        
        sem_post(&sem1);
        sem_getvalue(&sem1, &val2);
        logSyncMechSem(STDOUT_FILENO, *(int *) num, SYNC_OP_SEM_POST, SYNC_ROLE_CONSUMER, 0, val2);
    }

    logBankOfficeClose(STDOUT_FILENO, *(int *) num, pthread_self());
    return NULL;
}

void *pedidos(void *arg){
    tlv_request_t *request = (tlv_request_t *)malloc(sizeof(tlv_request_t));
     openServerFIFO();
    while(1){
        sem_wait(&sem1);
        read(srv_fifo_fd, request, sizeof(tlv_request_t));
            //printf("dsduusd\n");
        


        logRequest(STDOUT_FILENO, request->value.header.pid, request);

        buffer = *request;


        sem_post(&sem2);
    }
    return NULL;

}

int main(int argc, char *argv[]){
    if(argc != 3){
        printf("%s precisa de 2 argumentos", argv[0]);
        printf("Usage: %s <num_balcoes> <admin_pass>\n", argv[0]);
        exit(1);
    }

    srand(time(NULL));
    mode_t old_mask = umask(NEW_MASK);

    processArgs(argv);

    pthread_t balcao [num_threads];
    pthread_t pedido;

    int thrarg[num_threads];

    sem_init(&sem1, 0, num_threads);
    sem_getvalue(&sem1, &val1);
    logSyncMechSem(STDOUT_FILENO, MAIN_THREAD_ID, SYNC_OP_SEM_INIT, SYNC_ROLE_PRODUCER, 0, val1);
    
    sem_init(&sem2, 0, 0);
    sem_getvalue(&sem2, &val2);
    logSyncMechSem(STDOUT_FILENO, MAIN_THREAD_ID, SYNC_OP_SEM_INIT, SYNC_ROLE_PRODUCER, 0, val2);

    for(int i = 0; i < num_threads; i++){
        thrarg[i] = i+1;
        if(pthread_create(&balcao[i], NULL, balcaoEletronico, &thrarg[i]) != 0){
            perror("Nao foi possivel criar thread\n");
            exit(4);
        }
    }

    if(pthread_create(&pedido, NULL, pedidos, NULL) != 0){
        perror("Nao foi possivel criar thread de pedidos\n");
        exit(5);
    }

    logSyncMech(STDOUT_FILENO, MAIN_THREAD_ID, SYNC_OP_MUTEX_LOCK, SYNC_ROLE_ACCOUNT, 0);
    pthread_mutex_lock(&mut);
    logDelay(STDOUT_FILENO, MAIN_THREAD_ID, 0);
    createAccount(ADMIN_ACCOUNT_ID, 0, argv[2]);
    logAccountCreation(STDOUT_FILENO, MAIN_THREAD_ID, &bank_accounts[ADMIN_ACCOUNT_ID]);
    pthread_mutex_unlock(&mut);
    logSyncMech(STDOUT_FILENO, MAIN_THREAD_ID, SYNC_OP_MUTEX_UNLOCK, SYNC_ROLE_ACCOUNT, 0);

   

    pthread_join(pedido, NULL);

    for(int i = 0; i < num_threads; i++){
        pthread_join(balcao[i],NULL);
    }

    umask(old_mask);

    return 0;
}