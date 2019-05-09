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

bank_account_t bank_accounts[MAX_BANK_ACCOUNTS]; // Guarda as contas dos users
int num_accounts; //numero de contas no banco
bank_account_t bank_account; // buffer

sem_t sem; // Semaforo

int srv_fifo_fd;

int num_threads; // Numero de banco eletronicos
char admin_pass[MAX_PASSWORD_LEN + 1]; 

static const char caracteres[] = "0123456789abcdef";

char *generateSalt(){
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
        printf("%s nao foi possivel abrir\n", SERVER_FIFO_PATH);
        exit(1);
    }

    srv_fifo_fd = open(SERVER_FIFO_PATH, O_RDONLY);
}



void *balcaoEletronico(void *num){
    //pthread_mutex_lock(&mut);
    logBankOfficeOpen(STDOUT_FILENO, *(int *) num, pthread_self());
    //pthread_mutex_lock(&mut);
    int val;
    sem_getvalue(&sem, &val);
    logSyncMechSem(STDOUT_FILENO, *(int *) num, SYNC_OP_SEM_WAIT, SYNC_ROLE_CONSUMER, 0, val);
    sem_wait(&sem);

    return NULL;
}

void *pedidos(void *arg){
    int pedidos;
    return NULL;

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
    strcpy(admin_pass, args[2]);
    if(strlen(admin_pass) < MIN_PASSWORD_LEN || strlen(admin_pass) > MAX_PASSWORD_LEN){
        printf("Insira uma password entre %d e %d caracteres\n", MIN_PASSWORD_LEN, MAX_PASSWORD_LEN);
        exit(3);
    }
}

bank_account_t *createAccount(uint32_t account_id, uint32_t balance, char *pass){
    bank_account_t *bank_account = (bank_account_t *) malloc(sizeof(bank_account_t));
    
    bank_account->account_id = ADMIN_ACCOUNT_ID;

    bank_account->balance = balance;

    char *salt = generateSalt();
    strcpy(bank_account->salt, salt);

    char *hash = getHash(pass, salt);
    strcpy(bank_account->hash, hash); 

    return bank_account;
}

void addAccount(bank_account_t *bank, bank_account_t account, int *num_account){
    if(*num_account < MAX_BANK_ACCOUNTS){
        bank[*num_account] = account;
        *num_account += 1;
    }
}


int main(int argc, char *argv[]){
    if(argc != 3){
        printf("%s precisa de 2 argumentos", argv[0]);
        exit(1);
    }

    srand(time(NULL));
    mode_t old_mask = umask(NEW_MASK);

    processArgs(argv);

    pthread_t thread [num_threads];
    int *thrarg[num_threads];
    
    bank_account_t *admin_account = createAccount(0,0,admin_pass);
    addAccount(bank_accounts, *admin_account, &num_accounts);
    logAccountCreation(STDOUT_FILENO, MAIN_THREAD_ID, admin_account);
    

    for(int i = 0; i < num_threads; i++){
        thrarg[i] = (int *) malloc(sizeof(int));
        *thrarg[i] = i+1;
        if(pthread_create(&thread[i], NULL, balcaoEletronico, thrarg[i]) != 0){
            perror("Nao foi possivel criar thread\n");
            exit(4);
        }
    }

    sem_init(&sem, 0, 0);
    for(int i = 0; i < num_threads; i++){
        pthread_join(thread[i],NULL);
    }

    umask(old_mask);

    return 0;
}