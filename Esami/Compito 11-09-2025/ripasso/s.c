/*
Creare un servizio di chat in cui gli utenti possano registrarsi e accedere al servizio di chat 
e comunicare con utenti online.
Client: 
- Possono registrarsi utilizzando e-mail e password 
- quando gli utenti sono online Possono inviare un messaggio a un utente specifico (se connesso) 
- La comunicazione tra client e server deve essere TCP
- Nel caso in cui l'utente non è connesso il client potrà inviare i messaggi e il server invierà i messaggi non appena l'utente si connetterà 
Server principale: 
- Gestire la registrazione e l'autenticazione (NON TRAMITE FILE IL SERVER NON APPENA SI CHIUDE NON RICORDA PIU' NULLA / Quando il client si disconette)
- Gestire l'invio corretto del messaggio tra client A e client B
- Nel caso in cui l'utente sia offline salvare 10 messaggi dal più recente al meno recente e appena l'utente si connetterà verranno inviati 
- dovrà inviare le operazioni effettuate al server logging, in una comunicazione UDP conoscendo solo la porta, il server principale non è a conoscenza dell'indirizzo IP 
Server logging: 
- scriverà tutte le operazioni che vengono effettuate sul terminale e su un file operation.log .
La comunicazione tra client e server principale deve essere TCP (bidirezionale), 
la connessione tra server principale e server logging è UDP (unidirezionale)
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <pthread.h>

#define BUFFER_SIZE 1024
#define PORT 8080
#define PORT_LOG 7777
#define MAX_SIZE 100
#define MAX_CLIENT 10
#define MAX_MSG 10

typedef struct{
    char email[MAX_SIZE];
    char password[MAX_SIZE];
    char online_user[MAX_SIZE];
    char msg[MAX_SIZE];
    char operation[MAX_SIZE];
    int status;
}client_data;

typedef struct{
    int sockfd;
    struct sockaddr_in client_addr;
    char email[MAX_SIZE];
    char passowrd[MAX_SIZE];
    int status;
    //offline msg
    char msg_off[MAX_MSG][BUFFER_SIZE];
    int counter;
}client_info;

typedef struct{
    client_info* clients[MAX_CLIENT];
    int counter;
    pthread_mutex_t lock;
}client_list;

client_list clients = {.counter = 0, .lock = PTHREAD_MUTEX_INITIALIZER};

int sockfd_log;
struct sockaddr_in log_addr;

void send_logging(const char* msg){
    printf("[SERVER] Invio messaggio '%s' in broadcast\n", msg);

    if(sendto(sockfd_log, msg, strlen(msg), 0, (struct sockaddr*)&log_addr, sizeof(log_addr)) < 0){
        perror("sendto");
    }
}

void handle_client(void* arg){
    client_info* td = (client_info*)arg;
    int sockfd = td->sockfd;
    client_data data;
    char ip[INET_ADDRSTRLEN];
    char buffer[BUFFER_SIZE];

    inet_ntop(AF_INET, &td->client_addr.sin_addr, ip, INET_ADDRSTRLEN);
    int port = ntohs(td->client_addr.sin_port);

    
    while(1){
        ssize_t n = recv(sockfd, &data, sizeof(client_data), 0);
        if(n <= 0){
            if(n == 0){
                printf("Chiusura connessione\n");
            }
            perror("recv");
            break;
        }

        if(!strcmp(data.operation, "registrazione")){
            printf("[SERVER] Richiesta di registrazione da parte di %s\n", data.email);
            int found = 0;
            pthread_mutex_lock(&clients.lock);
            for(int i = 0; i < clients.counter; i++){
                if(!strcmp(clients.clients[i]->email, data.email) && !strcmp(clients.clients[i]->passowrd, data.password)){
                    found = 1;
                    break;
                }
            }

            if(!found && clients.counter < MAX_CLIENT){
                client_info* new_client = malloc(sizeof(client_info));
                strcpy(new_client->email, data.email);
                strcpy(new_client->passowrd, data.password);
                new_client->status = 0;
                new_client->sockfd = sockfd;
                new_client->counter = 0;
                clients.clients[clients.counter++] = new_client;
                printf("[SERVER] utente %s registrato\n", data.email);
                snprintf(buffer, sizeof(buffer), "REG_OK");
            }else{
                printf("Utente %s già registrato\n", data.email);
                snprintf(buffer, sizeof(buffer), "Utente già registrato");
            }
            pthread_mutex_unlock(&clients.lock);

            if(send(sockfd, buffer, strlen(buffer), 0) < 0){
                perror("send");
                exit(EXIT_FAILURE);
            }
            send_logging(buffer);
        }
        else if(!strcmp(data.operation, "login")){
            printf("[SERVER] Richiesta di login da parte di %s\n", data.email);
            int found = 0;
            
            pthread_mutex_lock(&clients.lock);
            for(int i = 0; i < clients.counter; i++){
                if(!strcmp(clients.clients[i]->email, data.email) && !strcmp(clients.clients[i]->passowrd, data.password)){
                    found = 1;
                    clients.clients[i]->status = 1;
                    for(int j = 0; j < clients.clients[i]->counter; j++){
                        if(send(clients.clients[i]->sockfd, clients.clients[i]->msg_off[j], strlen(clients.clients[i]->msg_off[j]), 0) < 0){
                            perror("send");
                            exit(EXIT_FAILURE);
                        }
                    }
                    clients.clients[i]->counter = 0;
                    memset(clients.clients[i]->msg_off, 0, sizeof(clients.clients[i]->msg_off));
                    break;
                }
            }

            pthread_mutex_unlock(&clients.lock);

            if(found){
                printf("[SERVER] Utente %s trovato\n", data.email);   
                snprintf(buffer, sizeof(buffer), "LOG_OK");
            }else{
                printf("[SERVER] Utente %s non trovato\n", data.email);
                snprintf(buffer, sizeof(buffer), "Login Fallito");
            }

            if(send(sockfd, buffer, strlen(buffer), 0) < 0){
                perror("send");
                exit(EXIT_FAILURE);
            }
            send_logging(buffer);
        }else if(!strcmp(data.operation, "messaggio")){
            printf("[SERVER] Richiesta di messaggio ricevuta da %s\n", data.email);
            
            int found = 0;
            char msg[BUFFER_SIZE];
            pthread_mutex_lock(&clients.lock);
            for(int i = 0; i < clients.counter; i++){
                if(!strcmp(clients.clients[i]->email, data.online_user)){
                    found = 1;
                    if(clients.clients[i]->status == 1){
                        printf("[SERVER] Utente %s online invio il messaggio\n", data.online_user);
                        snprintf(msg, sizeof(msg), "[%s]: %s\n", data.email, data.msg);
                        snprintf(buffer, sizeof(buffer), "MSG_OK");
                        if(send(clients.clients[i]->sockfd, msg, strlen(msg), 0) < 0){
                            perror("send");
                            exit(EXIT_FAILURE);
                        }
                    }else{
                        if(clients.clients[i]->counter < MAX_MSG){
                            printf("[SERVER] Utente %s trovato ma non online conservo il messaggio\n", data.online_user);
                            snprintf(clients.clients[i]->msg_off[clients.clients[i]->counter++], BUFFER_SIZE, "[%s]: %s", data.email, data.msg);
                            snprintf(buffer, sizeof(buffer), "MSG_SAVE");
                        }else{
                            printf("[SERVER] Casella piena\n");
                            snprintf(buffer, sizeof(buffer), "Casella di %s piena", data.online_user);
                        }
                    }
                    break;
                }else{
                    snprintf(buffer, sizeof(buffer), "Utente non trovato");
                }
            }
            pthread_mutex_unlock(&clients.lock);

            if(send(sockfd, buffer, strlen(buffer), 0) < 0){
                perror("send");
                exit(EXIT_FAILURE);
            }
            send_logging(buffer);    
        }else if(!strcmp(data.operation, "exit")){
            printf("Chiusura connessione\n");
            snprintf(buffer, sizeof(buffer), "exit");
            send_logging(buffer);
            break;
        }
    }

    pthread_mutex_lock(&clients.lock);
    for(int i = 0; i < clients.counter; i++) {
        if(clients.clients[i]->sockfd == sockfd) {
            free(clients.clients[i]);
            for(int j = i; j < clients.counter - 1; j++) {
                clients.clients[j] = clients.clients[j + 1];
            }
            clients.counter--;
            break;
        }
    }
    pthread_mutex_unlock(&clients.lock);
    close(sockfd);
    free(td);
}

int main(){
    int sockfd;
    struct sockaddr_in server_addr;

    if((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0){
        perror("socket");
        exit(EXIT_FAILURE);
    }

    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(PORT);
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);

    if(bind(sockfd, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0){
        perror("bind");
        exit(EXIT_FAILURE);
    }

    if(listen(sockfd, MAX_CLIENT) < 0){
        perror("listen");
        exit(EXIT_FAILURE);
    }

    printf("[SERVER] in ascolto su %d\n", PORT);

    if((sockfd_log = socket(AF_INET, SOCK_DGRAM, 0)) < 0){
        perror("socket");
        exit(EXIT_FAILURE);
    }

    int yes = 1;

    if(setsockopt(sockfd_log, SOL_SOCKET, SO_BROADCAST, &yes, sizeof(yes)) < 0){
        perror("setsockopt");
        exit(EXIT_FAILURE);
    }

    memset(&log_addr, 0, sizeof(log_addr));
    log_addr.sin_family = AF_INET;
    log_addr.sin_port = htons(PORT_LOG);   
    log_addr.sin_addr.s_addr = htonl(INADDR_BROADCAST);

    printf("[SERVER] Connesso al server logging in broadcast su porta %d\n", PORT_LOG);

    while(1){
        client_info* client = malloc(sizeof(client_info));
        socklen_t len = sizeof(client->client_addr);

        if((client->sockfd = accept(sockfd, (struct sockaddr*)&client->client_addr, &len)) < 0){
            perror("accept");
            free(client);
            continue;
        }

        pthread_t tid;
        if(pthread_create(&tid, NULL, (void*)handle_client, client) != 0){
            perror("pthread_create");
            close(client->sockfd);
            free(client);
        }
        
        pthread_detach(tid);
    }
    
    pthread_mutex_destroy(&clients.lock);
    close(sockfd_log);
    close(sockfd);
    return 0;
}