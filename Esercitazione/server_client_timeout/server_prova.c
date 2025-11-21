/*Il server:
ascolta su una porta prefissata (4028);
gestisce più client concorrenti tramite thread;
mantiene in memoria una struttura dati key–value (dizionario);
supporta i comandi testuali:
SET <chiave> <valore> → inserisce o aggiorna una coppia key–value; risponde OK o ERR.
GET <chiave> → restituisce il valore o NOTFOUND.
DEL <chiave> → elimina la chiave, risponde OK o NOTFOUND.
STATS → restituisce il numero di coppie e il tempo di uptime del server.
QUIT → chiude la connessione con BYE.
se il client non invia nulla entro 40 secondi, risponde ERR e chiude.
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <unistd.h>
#include <sys/select.h>
#include <sys/types.h>
#include <errno.h>
#include <pthread.h>


#define MAX_LENGTH 1024
#define BUFFER_SIZE 100
#define MAX_KEY 256
#define MAX_VAL 256
#define PORT 4028
#define TIMEOUT 40

struct kv{
    char key[MAX_KEY];
    char value[MAX_VAL];
    struct kv* next;
};


struct kv* head = NULL;
int counter = 0;
time_t server_start;
pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;

struct kv* search_list(const char* key){ //GET
    struct kv* found = head;
    while(found){
        if(!strcmp(found->key, key)){
            return found;
        }
        found = found->next;
    }
    return NULL;
}

int kv_set(const char* key, const char* value){ //SET
    if(pthread_mutex_lock(&lock) != 0){
        perror("pthread_mutex_lock");
        exit(EXIT_FAILURE);
    }

    struct kv* item = search_list(key);
    if(item){
        strncpy(item->value, value, MAX_VAL - 1);
        item->value[MAX_VAL - 1] = '\0';
    }else{
        struct kv* new = malloc(sizeof(struct kv));
        if(!new){
            perror("malloc");
            pthread_mutex_unlock(&lock);
            return 0;
        }

        strncpy(new->key, key, MAX_KEY-1);
        new->key[MAX_KEY - 1] = '\0';
        strncpy(new->value, value, MAX_VAL - 1);
        new->value[MAX_VAL - 1] = '\0';
        new->next = head;
        head = new;
        counter++;
    }
    if(pthread_mutex_unlock(&lock) != 0){
        perror("pthread_mutex_unlock");
        exit(EXIT_FAILURE);
    }
    return 1;
}

int delete(const char* key){ //del
    if(pthread_mutex_lock(&lock) != 0){
        perror("pthread_mutex_lock");
        exit(EXIT_FAILURE);
    }

    struct kv** node = &head;
    while(*node){
        if(!strcmp((*node)->key, key)){
            struct kv* temp = *node;
            *node = temp->next;
            free(temp);
            counter--;
            if(pthread_mutex_unlock(&lock) != 0){
                perror("pthread_mutex_unlock");
                exit(EXIT_FAILURE);
            }
            return 1;
        }
        node = &((*node)->next);
    }

    if(pthread_mutex_unlock(&lock) != 0){
        perror("pthread_mutex_unlock");
        exit(EXIT_FAILURE);
    }
    return 0;
}

void send_all(int sockfd, const char* buf, size_t len){
    size_t on = 0;
    while(on < len){
        ssize_t n = send(sockfd, buf + on, len - on, 0);
        if(n <= 0){
            if(errno == EINTR){
                continue;
            }
            perror("send");
            break;
        }
        on += n;
    }
}   

ssize_t recv_line(int sockfd, char* buffer, size_t maxlen, int timeout_client){
    size_t i = 0;
    fd_set fds;
    struct timeval tv;
    while(i < maxlen - 1){
        FD_ZERO(&fds);
        FD_SET(sockfd, &fds);
        tv.tv_sec = timeout_client;
        tv.tv_usec = 0;
        int rv = select(sockfd + 1, &fds, NULL, NULL, &tv);
        if(rv == 0){
            printf("TIMEOUT client chiudo connessione\n");
            send_all(sockfd, "BYE\n", 4);
        }else if(rv < 0){
            perror("select");
            exit(EXIT_FAILURE);
        }
        char c;
        ssize_t n = recv(sockfd, &c, 1, 0);
        if(n <= 0){
            return n;
        }
        if(c == '\n') break;
        buffer[i++] = c;
    }
    buffer[i] = '\0';
    return i;
}

void handle_client(void* arg){
    int sock = *(int*)arg;
    free(arg);
    char line[MAX_LENGTH];
    
    while(1){
        ssize_t n = recv_line(sock, line, MAX_LENGTH, TIMEOUT);
        
        if(n == -2){
            send_all(sock, "ERR\n", 4);
            break;
        }else if(n == 0){
            break;
        }
        else if(n == 0 || line[0] == '\0'){
            continue;
        }
        
        
        char* operation = strtok(line, " \n");
        char* key = strtok(NULL, " \n");
        char* value = strtok(NULL, "\n");

        if(!operation){
            send_all(sock, "ERR\n", 4);
            continue;
        }
        
        if(!strcmp(operation, "SET")){
            if(!key || !value || strchr(key, ' ')){
                send_all(sock, "ERR\n", 4);
                continue;
            }
            if(kv_set(key, value)){
                send_all(sock, "OK\n", 3);
            }else{
                send_all(sock, "ERR\n", 4);
            }
        }
        else if(!strcmp(operation, "GET")){
            if(!key || strchr(key, ' ')){
                send_all(sock, "ERR\n", 4);
                continue;
            }
            if(pthread_mutex_lock(&lock) != 0){
                perror("pthread_mutex_lock");
                exit(EXIT_FAILURE);
            }
            struct kv* found = search_list(key);
            if(found){
                char resp[MAX_VAL + 2];
                snprintf(resp, sizeof(resp), "%s\n", found->value);
                send_all(sock, resp, strlen(resp));
            }else{
                send_all(sock, "NOTFOUND\n", 9);
            }

            if(pthread_mutex_unlock(&lock) != 0){
                perror("pthread_mutex_unlock");
                exit(EXIT_FAILURE);
            }
        }else if(!strcmp(operation, "DEL")){
            if(!key || strchr(key, ' ')){
                send_all(sock, "ERR\n", 4);
                continue;
            }
            if(delete(key)){
                send_all(sock, "OK\n", 3);
            }else{
                send_all(sock, "NOTFOUND\n", 9);
            }
        }else if(!strcmp(operation, "STATS")){
            time_t now = time(NULL);
            int uptime = (int)(now - server_start);
            char response[BUFFER_SIZE];
            snprintf(response, sizeof(response), "COUNT=%d UPTIME=%d\n", counter, uptime);
            send_all(sock, response, strlen(response));
        }else if(!strcmp(operation, "QUIT")){
            send_all(sock, "BYE\n", 4);
            break;
        }else{
            send_all(sock, "ERR\n", 4);
        }
    }
    close(sock);
}

int main(int argc, char* argv[]){
    if(argc != 2){
        fprintf(stderr, "Usage: %s <port>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    int port = atoi(argv[1]);
    if(port != PORT){
        fprintf(stderr, "Porta non valida usare porta %d\n", PORT);
        exit(EXIT_FAILURE);
    }

    int sockfd;
    struct sockaddr_in server_addr;

    if((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0){
        perror("socket");
        exit(EXIT_FAILURE);
    }

    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);

    if(bind(sockfd, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0){
        perror("bind");
        exit(EXIT_FAILURE);
    }

    if(listen(sockfd, 10) < 0){
        perror("listen");
        exit(EXIT_FAILURE);
    }

    server_start = time(NULL);
    printf("Server avviato su porta %d, timeout %d sec\n", port, TIMEOUT);

    while(1){
        struct sockaddr_in client_addr;
        socklen_t len = sizeof(client_addr);
        int* clientfd = malloc(sizeof(int));
        if(!clientfd){
            continue;
        }
        *clientfd = accept(sockfd, (struct sockaddr*)&client_addr, &len);
        if(*clientfd < 0){
            perror("accept");
            continue;
        }

        pthread_t client_thread;
        if(pthread_create(&client_thread, NULL, (void*)handle_client, clientfd) != 0){
            perror("pthread_create");
            close(*clientfd);
            free(clientfd);
            exit(EXIT_FAILURE);
        }

        pthread_detach(client_thread);
    }
    close(sockfd);
}