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
pthread_mutex_t pedido_lock = PTHREAD_MUTEX_INITIALIZER; // Mutex
pthread_mutex_t bank_lock = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t buffer_lock = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t pedido_cond = PTHREAD_COND_INITIALIZER;
pthread_cond_t empty_cond = PTHREAD_COND_INITIALIZER;
pthread_cond_t cond = PTHREAD_COND_INITIALIZER;

sem_t sem1, sem2; // Semaforos
int val1, val2; // valor dos semaforos

bank_account_t bank_accounts[MAX_BANK_ACCOUNTS]; // Guarda as contas dos users
int num_accounts = 0; //numero de contas no banco

int srv_fifo_fd; // Descritor do fifo srv
int user_fifo_fd; // Descritor do fifo user
int fd_dummy;
char *user_fifo = ""; // Nome do user fifo

tlv_request_t buffer[MAX_BANK_ACCOUNTS]; // Buffer usado para comunicao entre Produtor e Consumidor
int bufin = 0;
int bufout = 0;

int num_threads; // Numero de banco eletronicos

int length = 0;

int slog; // fd 

int is_open = 1;

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

    char buf[MAX_PASSWORD_LEN];
    strcpy(buf,pass);
    strcat(buf, salt);

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

void makeServerFIFO(){
    if(mkfifo(SERVER_FIFO_PATH, FIFO_RW_MODE) != 0){
        if(errno == EEXIST){
            printf("Server FIFO - %s - ja existe\n", SERVER_FIFO_PATH);
        }
        else
            printf("%s nao foi possivel criar FIFO\n", SERVER_FIFO_PATH);
        unlink(SERVER_FIFO_PATH);
        exit(1);
    }
}

void openServerFIFO(){
    
    do{
        srv_fifo_fd = open(SERVER_FIFO_PATH, O_RDONLY);
        if(srv_fifo_fd == -1){
            sleep(1);
        }
    }while(srv_fifo_fd == -1);
    

    fd_dummy = open(SERVER_FIFO_PATH, O_WRONLY);
    
}

int openUserFIFO(pid_t pid){
    char *buf = malloc(sizeof(WIDTH_ID + 1));
    sprintf(buf, "%*d", WIDTH_ID, pid);

    user_fifo = malloc(USER_FIFO_PATH_LEN);

    strcpy(user_fifo, USER_FIFO_PATH_PREFIX);
    strcat(user_fifo, buf);
    
    user_fifo_fd = open(user_fifo, O_WRONLY);
    if(user_fifo_fd == -1){
        return -1;
    }

    return 0;
}

int login(uint32_t id, char *pass){
    char buf[MAX_PASSWORD_LEN];
    strcpy(buf,pass);

    for(int i = 0; i < num_accounts; i++){
        if(bank_accounts[i].account_id == id){
            char *hash = getHash(buf, bank_accounts[i].salt);
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

void put_request(tlv_request_t item){
    pthread_mutex_lock(&buffer_lock);
    buffer[bufin] = item;
    bufin = (bufin + 1) % MAX_BANK_ACCOUNTS;
    pthread_mutex_unlock(&buffer_lock);
    return;
} 

void get_request(tlv_request_t *itemp){
    pthread_mutex_lock(&buffer_lock);
    *itemp = buffer[bufout];
    bufout = (bufout + 1) % MAX_BANK_ACCOUNTS;
    pthread_mutex_unlock(&buffer_lock);
    return;
} 

int accountExists(uint32_t id){
    pthread_mutex_lock(&bank_lock);
        for(int i = 0; i < num_accounts; i++){
            if(bank_accounts[i].account_id == id){
                pthread_mutex_unlock(&bank_lock);
                return 1; // TRUE
            }
        }
    pthread_mutex_unlock(&bank_lock);
    return 0; // FALSE
}

bank_account_t *getBankAccount(uint32_t id){
    bank_account_t *account = (bank_account_t *)malloc(sizeof(bank_account_t));
    account = NULL;
    pthread_mutex_lock(&bank_lock);
    for(int i = 0; i < num_accounts; i++){
        if(bank_accounts[i].account_id == id){
            account = &bank_accounts[i];
            break;
        }
    }
    pthread_mutex_unlock(&bank_lock);
    return account;
}

void addAccount(bank_account_t account){
    pthread_mutex_lock(&bank_lock);
    bank_accounts[num_accounts] = account;
    num_accounts++;
    pthread_mutex_unlock(&bank_lock);
}

int createAccount(uint32_t account_id, uint32_t balance, char *pass){
     
    bank_account_t bank_account;

    char * buf = pass;
    
    if(accountExists(account_id)){
        
        return RC_ID_IN_USE;

    }
    
    bank_account.account_id = account_id;

    bank_account.balance = balance;

    char *salt = getSalt();
    strcpy(bank_account.salt, salt);

    char *hash = getHash(buf, salt);
    strcpy(bank_account.hash, hash);

    addAccount(bank_account);
    
    return RC_OK;
}

int transfer(uint32_t source_id, uint32_t balance, uint32_t dest_id, uint32_t delay){

    if(!accountExists(source_id)){
        return RC_ID_NOT_FOUND;
    }

    if(source_id == dest_id){
        return RC_SAME_ID;
    }

    bank_account_t *b_s, *b_d;
    b_s = getBankAccount(source_id);
    b_d = getBankAccount(dest_id);
    if(b_d == NULL){
        return RC_OTHER;
    }
    usleep(delay);
    if(b_s->balance < balance){
        return RC_NO_FUNDS;
    }

    if(b_d->balance + balance > MAX_BALANCE){
        return RC_TOO_HIGH;
    }

    b_s->balance -= balance;
    b_d->balance += balance;

    return RC_OK;
}

int getBalance(uint32_t account_id){
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
    if(fchmod(srv_fifo_fd, FIFO_READ_MODE) != 0){
        printf("Nao foi possivel fechar o FIFO %s\n", SERVER_FIFO_PATH);
        exit(4);
    }
    is_open = 0;
}


void *balcaoEletronico(void *num){

    logBankOfficeOpen(STDOUT_FILENO, *(int *) num, pthread_self());
    
    while(is_open) {
        
        sem_getvalue(&sem2, &val1);
        logSyncMechSem(STDOUT_FILENO, *(int *) num, SYNC_OP_SEM_WAIT, SYNC_ROLE_CONSUMER, 0, val1);
        sem_wait(&sem2);
        tlv_reply_t tlv_reply;
        tlv_request_t request;
        get_request(&request);

        tlv_reply.type = request.type;

        if(login(request.value.header.account_id, request.value.header.password)){

            switch(request.type){


                /**     CREATE ACCOUNT      **/

                case OP_CREATE_ACCOUNT:
                usleep(request.value.header.op_delay_ms * 1000);
                length += sizeof(int) + sizeof(int);
                if(request.value.header.account_id == ADMIN_ACCOUNT_ID){
                    int rc = createAccount(request.value.create.account_id, request.value.create.balance, request.value.create.password);
                    tlv_reply.value.header.ret_code = rc;
                    if(rc == RC_OK)
                        logAccountCreation(STDOUT_FILENO, *(int *) num, &bank_accounts[num_accounts - 1]);
                }
                else if(request.value.header.account_id != ADMIN_ACCOUNT_ID){
                    tlv_reply.value.header.ret_code = RC_OP_NALLOW;
                }

                else{
                    tlv_reply.value.header.ret_code = RC_OTHER;
                }

                tlv_reply.value.header.account_id = *(int *)num;
                length += sizeof(*(int *)num) + sizeof(tlv_reply.value.header.ret_code);

                break;


                /**     CHECK BALANCE       **/

                case OP_BALANCE:
                usleep(request.value.header.op_delay_ms * 1000);
                length += sizeof(int) + sizeof(int);
                //tlv_reply.value.balance.balance = 0;

                if(request.value.header.account_id != ADMIN_ACCOUNT_ID){
                    tlv_reply.value.balance.balance = getBalance(request.value.header.account_id);   
                    length += sizeof(int);
                }

                
                else if(request.value.header.account_id == ADMIN_ACCOUNT_ID){
                    tlv_reply.value.header.ret_code = RC_OP_NALLOW;
                }
                else{
                    tlv_reply.value.header.ret_code = RC_OTHER;
                }

                break;


                /**     SERVER SHUTDOWN     **/

                case OP_SHUTDOWN:
                usleep(request.value.header.op_delay_ms * 1000);
                length += sizeof(request.value.header.account_id) + sizeof(int);

                if(request.value.header.account_id == ADMIN_ACCOUNT_ID){
                    shutdown();
                    tlv_reply.value.header.ret_code = RC_OK;
                    tlv_reply.value.shutdown.active_offices = bufin - bufout;
                    length += sizeof(int);
                }

                else if(request.value.header.account_id != ADMIN_ACCOUNT_ID){
                    tlv_reply.value.header.ret_code = RC_OP_NALLOW;
                }

                else{
                    tlv_reply.value.header.ret_code = RC_OTHER;
                }
                
                break;


                /**     TRANSFERENCIA       **/

                case OP_TRANSFER:
                length += sizeof(int) + sizeof(int);
                if(request.value.header.account_id == ADMIN_ACCOUNT_ID){
                    tlv_reply.value.header.ret_code = RC_OP_NALLOW;
                }

                else if(request.value.header.account_id != ADMIN_ACCOUNT_ID){
                    tlv_reply.value.header.ret_code =
                    transfer(request.value.header.account_id, request.value.transfer.amount, request.value.transfer.account_id, request.value.header.op_delay_ms * 1000);
                    length += sizeof(request.value.transfer.amount);
                }
                else{
                    tlv_reply.value.header.ret_code = RC_OTHER;
                }

                bank_account_t *account = getBankAccount(request.value.header.account_id);
                tlv_reply.value.transfer.balance = (*account).balance;

                break;
                
                /**     DEFAULT     **/

                default:
                break;
            }
        }

        else{
            length += 2*sizeof(int);
            tlv_reply.value.header.account_id = *(int *)num;
            tlv_reply.value.header.ret_code = RC_LOGIN_FAIL;
        }

        tlv_reply.length = length;
        
        if(openUserFIFO(request.value.header.pid) != 0){
            tlv_reply.value.header.ret_code = RC_USR_DOWN;
        }
        else{
            write(user_fifo_fd, &tlv_reply, sizeof(tlv_reply_t));

        }

        logReply(STDOUT_FILENO, *(int *)num, &tlv_reply);

        close(user_fifo_fd);
        length = 0;

        sem_post(&sem1);
        sem_getvalue(&sem1, &val2);
        logSyncMechSem(STDOUT_FILENO, *(int *) num, SYNC_OP_SEM_POST, SYNC_ROLE_CONSUMER, request.value.header.pid, val2);
    }

    logBankOfficeClose(STDOUT_FILENO, *(int *) num, pthread_self());

    return NULL;
}

void destroy(){
    pthread_mutex_destroy(&mut);
    pthread_mutex_destroy(&pedido_lock);
    pthread_mutex_destroy(&bank_lock);
    pthread_mutex_destroy(&buffer_lock);
}

int main(int argc, char *argv[]){
    if(argc != 3){
        printf("%s precisa de 2 argumentos\n", argv[0]);
        printf("Usage: %s <num_balcoes> <admin_pass>\n", argv[0]);
        exit(1);
    }

    srand(time(NULL));
    
    mode_t old_mask = umask(NEW_MASK);

    processArgs(argv);

    slog = open("slog.txt", O_WRONLY | O_APPEND | O_CREAT, 0777);
    //dup2(slog, STDOUT_FILENO); // Output redirecionado para <file.txt>

    

    pthread_t balcao[num_threads]; //

    int thrarg[num_threads]; // numero do balcao correspondente

    sem_init(&sem1, 0, num_threads);
    sem_getvalue(&sem1, &val1);
    logSyncMechSem(STDOUT_FILENO, MAIN_THREAD_ID, SYNC_OP_SEM_INIT, SYNC_ROLE_PRODUCER, 0, val1);
    
    sem_init(&sem2, 0, 0);
    sem_getvalue(&sem2, &val2);
    logSyncMechSem(STDOUT_FILENO, MAIN_THREAD_ID, SYNC_OP_SEM_INIT, SYNC_ROLE_PRODUCER, 0, val2);

    for(int i = 0; i < num_threads; i++){
        thrarg[i] = i+1;
        if(pthread_create(&balcao[i], NULL, balcaoEletronico, &thrarg[i]) != 0){
            printf("Nao foi possivel criar thread balcao %d\n", i);
            exit(4);
        }
    }

    logSyncMech(STDOUT_FILENO, MAIN_THREAD_ID, SYNC_OP_MUTEX_LOCK, SYNC_ROLE_ACCOUNT, 0);
    pthread_mutex_lock(&mut);
    logDelay(STDOUT_FILENO, MAIN_THREAD_ID, 0);
    
    createAccount(ADMIN_ACCOUNT_ID, 0, argv[2]);

    logAccountCreation(STDOUT_FILENO, MAIN_THREAD_ID, &bank_accounts[ADMIN_ACCOUNT_ID]);
    
    pthread_mutex_unlock(&mut);
    logSyncMech(STDOUT_FILENO, MAIN_THREAD_ID, SYNC_OP_MUTEX_UNLOCK, SYNC_ROLE_ACCOUNT, 0);
    
    makeServerFIFO();
    openServerFIFO();
    
    // ==== Le pedidos de user até o FIFO SRV estiver aberto para escrita =====
    while(is_open){
        tlv_request_t request;

        if(read(srv_fifo_fd, &request, sizeof(tlv_request_t)) > 0){
            sem_getvalue(&sem1, &val1);
            logSyncMechSem(STDOUT_FILENO, MAIN_THREAD_ID, SYNC_OP_SEM_WAIT, SYNC_ROLE_PRODUCER, request.value.header.pid, val1);
            sem_wait(&sem1);
            logRequest(STDOUT_FILENO, request.value.header.pid, &request);
            put_request(request);
            
            sem_post(&sem2);
            sem_getvalue(&sem2,&val2);
            logSyncMechSem(STDOUT_FILENO, MAIN_THREAD_ID, SYNC_OP_SEM_POST, SYNC_ROLE_PRODUCER, request.value.header.pid, val2);
        }

        if(request.type == OP_SHUTDOWN){
            break;
        }
    }
    // =======================================================================


    // ====== Espera pelos balcoes encerrarem =======

    for(int i = 0; i < num_threads; i++){
        pthread_join(balcao[i],NULL);
    }

    // ==============================================



    // ======== Fecha e apaga FIFO SRV ==============

    close(srv_fifo_fd);

    unlink(SERVER_FIFO_PATH);

    // ==============================================

    destroy();

    umask(old_mask);

    return 0;
}