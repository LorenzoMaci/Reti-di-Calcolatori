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
- scriverà tutte le operazioni che vengono effettuate sul terminale.
La comunicazione tra client e server principale deve essere TCP (bidirezionale), 
la connessione tra server principale e server logging è UDP (unidirezionale)
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define BUFFER_SIZE 1024
#define MAX_SIZE 100
#define MAX_MESSAGE 10
#define MAX_USER 10
#define RECV_PORT 8080
#define PORT 8081

typedef struct{
    char email[MAX_SIZE];
    char password[MAX_SIZE];
    char operation[MAX_SIZE];
    char useronline[MAX_SIZE];
    char msg[MAX_SIZE];
    int status;
}client_data;

typedef struct {
    int sockfd;
    struct sockaddr_in addr;
    char email[MAX_SIZE];
    char password[MAX_SIZE];
    int status;

    //utente offline
    char msg_off[MAX_MESSAGE][BUFFER_SIZE];
    int counter_off;
} client_info;

typedef struct{
    client_info* clients[MAX_USER];
    int counter;
    pthread_mutex_t lock;
}client_list;

client_list clients = {.counter = 0, .lock = PTHREAD_MUTEX_INITIALIZER};

void send_loggin(const char* msg){
    int sockfd;
    struct sockaddr_in loggin_addr;

    if((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0){
        perror("socket");
        exit(EXIT_FAILURE);
    }

    int yes = 1;
    if(setsockopt(sockfd, SOL_SOCKET, SO_BROADCAST, &yes, sizeof(yes)) < 0){
        perror("setsockop");
        exit(EXIT_FAILURE);
    }

    memset(&loggin_addr, 0, sizeof(loggin_addr));
    loggin_addr.sin_family = AF_INET;
    loggin_addr.sin_port = htons(PORT);
    loggin_addr.sin_addr.s_addr = htonl(INADDR_BROADCAST);

    if(sendto(sockfd, msg, strlen(msg), 0, (struct sockaddr*)&loggin_addr, sizeof(loggin_addr)) < 0){
        perror("sendto");
    }
    close(sockfd);
}

void handle_client(void* arg){
    client_info* td = (client_info*)arg;
    int sockfd = td->sockfd;
    char buffer[BUFFER_SIZE];
    char msg[BUFFER_SIZE];
    char client_ip[INET_ADDRSTRLEN];

    inet_ntop(AF_INET, &td->addr.sin_addr, client_ip, INET_ADDRSTRLEN);
    int client_port = ntohs(td->addr.sin_port);
    printf("New connection from %s:%d\n", client_ip, client_port);

    while(1){
        client_data data;
        ssize_t n = recv(sockfd, &data, sizeof(client_data), 0);
        if(n <= 0){
            if(n == 0){
                printf("Chiudo la connessionen\n");
            }
            perror("recv");
            break;
        }

        if(!strcmp(data.operation, "registrazione")){
            printf("[SERVER] registrazione richiesta\n");
            pthread_mutex_lock(&clients.lock);
            int found = 0;
            for(int i = 0; i < clients.counter; i++){
                if(!strcmp(clients.clients[i]->email, data.email)){
                    found = 1;
                    break;
                }
            }

            if(!found && clients.counter < MAX_USER){
                client_info* new_client = malloc(sizeof(client_info));
                strcpy(new_client->email, data.email);
                strcpy(new_client->password, data.password);
                new_client->status = 0;
                new_client->sockfd = sockfd;
                new_client->counter_off = 0;
                clients.clients[clients.counter++] = new_client;
                snprintf(buffer, sizeof(buffer), "REG_OK");
                printf("[SERVER] Registrazione avvenuta con successo");
            }else{
                printf("Utente già registrato\n");
                snprintf(buffer, sizeof(buffer), "Utente già registrato");
            }

            pthread_mutex_unlock(&clients.lock);

            if(send(sockfd, buffer, strlen(buffer), 0) < 0){
                perror("send");
                exit(EXIT_FAILURE);
            } 
            send_loggin(buffer);
        }
        else if(!strcmp(data.operation, "login")){
            printf("[SERVER] Login richiesto\n");
            
            pthread_mutex_lock(&clients.lock);
            int found = 0;
            for(int i = 0; i < clients.counter; i++){
                if(!strcmp(clients.clients[i]->email, data.email) && !strcmp(clients.clients[i]->password, data.password)){
                    clients.clients[i]->status = 1;
                    found = 1;
                    clients.clients[i]->sockfd = sockfd;
                    for(int j = 0; j < clients.clients[i]->counter_off; j++){
                        if(send(sockfd, clients.clients[i]->msg_off[j], strlen(clients.clients[i]->msg_off[j]), 0) < 0){
                            perror("send");
                            exit(EXIT_FAILURE);
                        }
                    }
                    clients.clients[i]->counter_off = 0;
                    memset(clients.clients[i]->msg_off, 0, sizeof(clients.clients[i]->msg_off));
                }
            }
            pthread_mutex_unlock(&clients.lock);
            if(found){
                printf("[SERVER] Utente %s trovato\n", data.email);   
                snprintf(buffer, sizeof(buffer), "LOG_OK");
            }else{
                printf("[SERVER] Utente %s non trovato\n", data.email);
                snprintf(buffer, sizeof(buffer), "LOG_FAILED");
            }

            if(send(sockfd, buffer, strlen(buffer), 0) < 0){
                perror("send");
                exit(EXIT_FAILURE);
            }
            send_loggin(buffer);
        }
        else if(!strcmp(data.operation, "messaggio")){
            printf("[SERVER] Ricerca Utente a cui inviare il messaggio\n");
            
            pthread_mutex_lock(&clients.lock);
            client_info* found = NULL;
            
            for(int i = 0; i < clients.counter; i++){
                if(!strcmp(clients.clients[i]->email, data.useronline)){
                    found = clients.clients[i];
                    break;
                }
            }

            pthread_mutex_unlock(&clients.lock);

            if(found != NULL){
                if(found->status == 1){
                    printf("[Server] Utente %s Online invio il messaggio\n", data.useronline);
                    snprintf(buffer, sizeof(buffer), "[%s]: %s", data.email, data.msg);
                    if(send(found->sockfd, buffer, strlen(buffer), 0) < 0){
                        perror("send");
                        exit(EXIT_FAILURE);
                    }
                    snprintf(msg, sizeof(msg), "MSG_OK");
                }else{
                    if(found->counter_off < MAX_MESSAGE){
                        snprintf(found->msg_off[found->counter_off++], BUFFER_SIZE, "[%s]: %s", data.email, data.msg);
                        printf("[SERVER] Utente %s offline, messaggio salvato\n", data.useronline);
                        snprintf(msg, sizeof(msg), "MSG_SAVE");
                    }else{
                        printf("[SERVER] Casella messaggio piena\n");
                        snprintf(msg, sizeof(msg), "FULL_MSG");
                    }
                }
            }else{
                snprintf(msg, sizeof(msg), "Utente non trovato");
            }
            if(send(sockfd, msg, strlen(msg), 0 ) < 0){
                    perror("send");
                    exit(EXIT_FAILURE);
            }
            send_loggin(msg);
        }
        else if(!strcmp(data.operation, "exit")){
            printf("Chiusura Connessione\n");
            snprintf(buffer, sizeof(buffer), "exit");
            if(send(sockfd, buffer, strlen(buffer), 0 ) < 0){
                perror("send");
                exit(EXIT_FAILURE);
            }
            send_loggin(buffer);
            break;
        }
    }

    // remove client from list
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
    struct sockaddr_in recv_addr;

    if((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0){
        perror("bind");
        exit(EXIT_FAILURE);
    }

    memset(&recv_addr, 0, sizeof(recv_addr));
    recv_addr.sin_family = AF_INET;
    recv_addr.sin_port = htons(RECV_PORT);
    recv_addr.sin_addr.s_addr = htonl(INADDR_ANY);

    if(bind(sockfd, (struct sockaddr*)&recv_addr, sizeof(recv_addr)) < 0){
        perror("bind");
        exit(EXIT_FAILURE);
    }

    if(listen(sockfd, MAX_USER) < 0){
        perror("listen");
        exit(EXIT_FAILURE);
    }

    printf("[SERVER] in ascolto sulla porta %d\n", RECV_PORT);
    
    while(1){
        client_info* client = malloc(sizeof(client_info));
        socklen_t len = sizeof(client->addr);

        if((client->sockfd = accept(sockfd, (struct sockaddr*)&client->addr, &len)) < 0){
            perror("accept");
            free(client);
            continue;
        }

        pthread_t client_thread;
        if(pthread_create(&client_thread, NULL, (void*)handle_client, client) != 0){
            perror("pthread_create");
            close(client->sockfd);
            free(client);
        }
        pthread_detach(client_thread);
    }

    close(sockfd);
    pthread_mutex_destroy(&clients.lock);
}