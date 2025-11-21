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
#include <sys/socket.h>
#include <pthread.h>
#include <arpa/inet.h>
#include <netinet/in.h>

#define BUFFER_SIZE 1024
#define PORT 8080
#define MAX_SIZE 100
#define MAX_CLIENT 10

typedef struct{
    char email[MAX_SIZE];
    char password[MAX_SIZE];
    char useronline[MAX_SIZE];
    char msg[MAX_SIZE];
    char operation[MAX_SIZE];
    int status; //0 = off, 1 = online
    char ip[INET_ADDRSTRLEN];
    int port_udp;
}client_data;
 
typedef struct{
    int sockfd;
    struct sockaddr_in addr;
    char email[MAX_SIZE];
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
FILE* f;

int login_user(client_data* td, FILE* f){
    rewind(f);
    char buffer[BUFFER_SIZE];
    char email[MAX_SIZE], password[MAX_SIZE];
    
    while(fgets(buffer, BUFFER_SIZE, f)){
        if(sscanf(buffer, "%s %s", email, password) == 2){
            if(!strcmp(td->email, email) && !strcmp(td->password, password)){
                return 1;
            }
        }
    }
    return 0;
}

int reg(client_data* td, FILE* f){
    if(login_user(td, f)){
        return 0;
    }
    fseek(f, 0, SEEK_END);
    fprintf(f, "%s %s\n", td->email, td->password);
    fflush(f);
    return 1;
}

void handle_client(void* arg){
    client_info *td = (client_info *)arg;
    int client_sock = td->sockfd;
    char buffer[BUFFER_SIZE];
    char client_ip[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &td->addr.sin_addr, client_ip, INET_ADDRSTRLEN);
    int client_port = ntohs(td->addr.sin_port);
    printf("New connection from %s:%d\n", client_ip, client_port);

    while(1){
        client_data data;
        ssize_t n = recv(client_sock, &data, sizeof(client_data), 0);
        if(n <= 0){
            if(n == 0){
                printf("Connessione chiusa\n");
            }
            perror("recv");
            exit(EXIT_FAILURE);
        }

        if(!strcmp(data.operation, "registrazione")){
            if(reg(&data, f)){
                printf("Registrazione avvenuta con successo per %s (P2P: %s:%d)\n", data.email, data.ip, data.port_udp);
                pthread_mutex_lock(&clients.lock);
                
                int found = 0;
                for(int i = 0; i < clients.counter; i++){
                    if(!strcmp(clients.clients[i]->email, data.email)){
                        found = 1;
                        break;
                    }
                }

                if(!found && clients.counter < MAX_CLIENT){
                    client_info* new_clients = malloc(sizeof(client_info));
                    new_clients->sockfd = client_sock;
                    strcpy(new_clients->email, data.email);
                    strcpy(new_clients->ip, data.ip);
                    new_clients->port_udp = data.port_udp;
                    new_clients->status = 0;
                    clients.clients[clients.counter++] = new_clients;
                }

                pthread_mutex_unlock(&clients.lock);
                snprintf(buffer, sizeof(buffer), "REG_OK");
            }else{
                printf("Utente %s già registrato\n", data.email);
                snprintf(buffer, sizeof(buffer), "Utente già registrato");
            }
        }else if(!strcmp(data.operation, "login")){
            if(login_user(&data, f)){
                pthread_mutex_lock(&clients.lock);

                for(int i = 0; i < clients.counter; i++){
                    if(!strcmp(clients.clients[i]->email, data.email)){
                        clients.clients[i]->status = 1;
                    }
                }

                pthread_mutex_unlock(&clients.lock);
                
                printf("Login avvenuto con successo per %s\n", data.email);
                snprintf(buffer, sizeof(buffer), "LOG_OK");
            }else{
                printf("Login Fallito per %s\n", data.email);
                snprintf(buffer, sizeof(buffer), "login fallito");
            }
        }
        else if(!strcmp(data.operation, "messaggio")){
            pthread_mutex_lock(&clients.lock);
            int found = 0;
            for(int i = 0; i < clients.counter; i++){
                if(!strcmp(clients.clients[i]->email, data.useronline)){
                    found = 1;
                    if(clients.clients[i]->status == 1){
                        snprintf(buffer, sizeof(buffer), "USER_ON %s %d", clients.clients[i]->ip, clients.clients[i]->port_udp);
                    }else{
                        snprintf(buffer, sizeof(buffer), "Utente offline");
                    }
                    break;
                }
            }
            if(!found){
                snprintf(buffer, sizeof(buffer), "Utente non trovato");
            }
            pthread_mutex_unlock(&clients.lock);
        }else if(!strcmp(data.operation, "exit")){
            snprintf(buffer, sizeof(buffer), "exit");
            if(send(client_sock, buffer, strlen(buffer), 0) < 0){
                perror("send");
                exit(EXIT_FAILURE);
            }
            break;
        }

        if(send(client_sock, buffer, strlen(buffer), 0) < 0){
            perror("send");
            exit(EXIT_FAILURE);
        }
    }
    close(client_sock);
}

int main(){
    int sockfd;
    struct sockaddr_in recv_addr;

    if((f = fopen("database.txt", "a+")) == NULL){
        perror("fopen");
        exit(EXIT_FAILURE);
    }

    if((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0){
        perror("socket");
        exit(EXIT_FAILURE);
    }

    memset(&recv_addr, 0, sizeof(recv_addr));
    recv_addr.sin_family = AF_INET;
    recv_addr.sin_port = htons(PORT);
    recv_addr.sin_addr.s_addr = htonl(INADDR_ANY);

    if(bind(sockfd, (struct sockaddr*)&recv_addr, sizeof(recv_addr)) < 0){
        perror("bind");
        exit(EXIT_FAILURE);
    }

    if(listen(sockfd, MAX_CLIENT) < 0){
        perror("listen");
        exit(EXIT_FAILURE);
    }

    printf("[SERVER] in ascolto sulla porta %d\n", PORT);

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
            exit(EXIT_FAILURE);
        }
        
        pthread_detach(client_thread);
    }
    pthread_mutex_destroy(&clients.lock);
    close(sockfd);
}