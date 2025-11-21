/*
Creare un servizio di chat in cui gli utenti possano registrarsi e accedere al servizio di chat 
e comunicare con utenti specifici tramite comunicazione P2P. 
Client: 
- Possono registrarsi utilizzando e-mail e password 
- Possono inviare un messaggio a un utente specifico (se connesso) 
- Per comunicare con altri utenti, il client deve utilizzare una comunicazione non affidabile (UDP)
- I client pubblicano al server la propria porta UDP
Server principale: 
- Gestire la registrazione e l'autenticazione   
- Salvare utenti in un database (file.txt) e ricaricarlo all’avvio
- Fornire ai client le info necessarie per il P2P (IP e porta)
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>

#define BUFFER_SIZE 1024
#define MAX_SIZE 100
#define MAX_CLIENT 10
#define PORT 7070

typedef struct{
    char email[MAX_SIZE];
    char password[MAX_SIZE];
    char user_online[MAX_SIZE];
    char msg[MAX_SIZE];
    char operation[MAX_SIZE];
    char ip[INET_ADDRSTRLEN];
    int port_udp;
    int status; 
}client_data;

typedef struct{
    int sockfd;
    struct sockaddr_in client_addr;
    char email[MAX_SIZE];
    char password[MAX_SIZE];
    char ip[INET_ADDRSTRLEN];
    int port_udp;
    int status;
}client_info;

typedef struct{
    client_info* clients[MAX_CLIENT];
    int counter;
    pthread_mutex_t lock;
}client_list;

client_list clients = {.counter = 0, .lock = PTHREAD_MUTEX_INITIALIZER};


void handle_client(void* arg){
    client_info* td = (client_info*)arg;
    int sockfd = td->sockfd;
    char client_ip[INET_ADDRSTRLEN];
    client_data data;
    char buffer[BUFFER_SIZE];

    inet_ntop(AF_INET, &td->client_addr.sin_addr, client_ip, INET_ADDRSTRLEN);
    int port = ntohs(td->client_addr.sin_port);    

    printf("[SERVER] nuova connessione da %s:%d\n", client_ip, port);

    while(1){
        ssize_t n = recv(sockfd, &data, sizeof(client_data), 0);
        if(n <= 0){
            if(n == 0){
                printf("Chiusura connessione\n");
            }
            perror("recv");
            exit(EXIT_FAILURE);
        }

        if(!strcmp(data.operation, "registrazione")){
            printf("[SERVER] richiesta registrazione\n");

            pthread_mutex_lock(&clients.lock);

            int found = 0;
            for(int i = 0; i < clients.counter; i++){
                if(!strcmp(clients.clients[i]->email, data.email) && !strcmp(clients.clients[i]->password, data.password)){
                    found = 1;
                    break;
                }
            }

            if(!found &&  clients.counter < MAX_CLIENT){
                client_info* new_client = malloc(sizeof(client_info));
                strcpy(new_client->email, data.email);
                strcpy(new_client->password, data.password);
                strcpy(new_client->ip, data.ip);
                new_client->port_udp = data.port_udp;
                new_client->status = 0;
                new_client->sockfd = sockfd;
                clients.clients[clients.counter++] = new_client;
                printf("[SERVER] Utente %s registrato\n", data.email);
                snprintf(buffer, sizeof(buffer), "REG_OK");  
            }else{
                snprintf(buffer, sizeof(buffer), "Utente già registrato");
            }

            pthread_mutex_unlock(&clients.lock);
        }
        else if(!strcmp(data.operation, "login")){
            printf("[SERVER] richiesto login\n");

            pthread_mutex_lock(&clients.lock);
            int found = 0;
            for(int i = 0; i < clients.counter; i++){
                if(!strcmp(clients.clients[i]->email, data.email) && !strcmp(clients.clients[i]->password, data.password)){
                    found = 1;
                    clients.clients[i]->status = 1;
                    printf("[SERVER] login per %s avvenuto con successo\n", data.email);
                    break;
                }
            }

            if (!found && clients.counter < MAX_CLIENT) {
                client_info *new_client = malloc(sizeof(client_info));
                strcpy(new_client->email, data.email);
                strcpy(new_client->password, data.password);
                strcpy(new_client->ip, data.ip);
                new_client->port_udp = data.port_udp;
                new_client->status = 1;
                new_client->sockfd = sockfd;
                clients.clients[clients.counter++] = new_client;
            }

            pthread_mutex_unlock(&clients.lock);

            if(found){
                snprintf(buffer, sizeof(buffer), "LOG_OK");
            }else{
                snprintf(buffer, sizeof(buffer), "Login fallito");
            }
        }
        else if(!strcmp(data.operation, "messaggio")){
            printf("[SERVER] ricerca utente per inviare il messaggio\n");

            pthread_mutex_lock(&clients.lock);
            int found = 0;
            for(int i = 0; i < clients.counter; i++){
                if(!strcmp(clients.clients[i]->email, data.user_online)){
                    found = 1;
                    if(clients.clients[i]->status == 1){
                        printf("Utente %s online invio IP: %s e Porta: %d\n",data.user_online, clients.clients[i]->ip, clients.clients[i]->port_udp);
                        snprintf(buffer, sizeof(buffer), "USER_ON %s %d", clients.clients[i]->ip, clients.clients[i]->port_udp);
                    }else{
                        printf("Utente %s offline\n", data.user_online);
                        snprintf(buffer, sizeof(buffer), "Utente %s offline" ,data.user_online);
                    }
                    break;
                }
            }

            pthread_mutex_unlock(&clients.lock);

            if(!found){
                snprintf(buffer, sizeof(buffer), "Utente  non trovato");
            }
        }
        else if(!strcmp(data.operation, "exit")){
            printf("[SERVER] Chiusura connessione\n");
            break;
        }

        if(send(sockfd, buffer, strlen(buffer), 0) < 0){
            perror("send");
            exit(EXIT_FAILURE);
        }
    }
    close(sockfd);
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
    close(sockfd);
}
