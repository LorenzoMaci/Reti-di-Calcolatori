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
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <pthread.h>

#define BUFFER_SIZE 1024
#define MAX_SIZE 100
#define MAX_CLIENTS 10
#define PORT 5555

typedef struct{
    char email[MAX_SIZE];
    char password[MAX_SIZE];
    char online_user[MAX_SIZE];
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
    client_info* clients[MAX_CLIENTS];
    int counter;
    pthread_mutex_t lock;
}client_list;

client_list clients = {.counter = 0, .lock = PTHREAD_MUTEX_INITIALIZER};
FILE* f;


int login(FILE* f, client_data td){
    rewind(f);
    char buffer[BUFFER_SIZE];
    char email[MAX_SIZE],  password[MAX_SIZE];
    while(fgets(buffer, BUFFER_SIZE, f)){
        if(sscanf(buffer, "%s %s", email, password) == 2){
            if(!strcmp(td.email, email) && !strcmp(td.password, password)){
                return 1;
            }
        }
    }
    return 0;
}

int registration(FILE* f, client_data td){
    if(login(f, td)){
        return 0;
    }
    fseek(f, 0, SEEK_END);
    fprintf(f, "%s %s\n", td.email, td.password);
    fflush(f);
    return 1;
}

void handle_data(void* arg){
    client_info* td = (client_info*)arg;
    int sockfd = td->sockfd;
    char buffer[BUFFER_SIZE];
    char ip[INET_ADDRSTRLEN];
    client_data data;
    
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
            if(registration(f, data)){
                printf("[SERVER] Regitrazione avvenuto con successo per '%s' P2P (%s:%d)\n", data.email, data.ip, data.port_udp);

                pthread_mutex_lock(&clients.lock);
                int found = 0;
                for(int i = 0; i < clients.counter; i++){
                    if(!strcmp(clients.clients[i]->email, data.email) && !strcmp(clients.clients[i]->password, data.password)){
                        found = 1;
                        break;
                    }
                }

                if(!found && clients.counter < MAX_CLIENTS){
                    client_info* new_client = malloc(sizeof(client_info));
                    strcpy(new_client->email, data.email);
                    strcpy(new_client->password, data.password);
                    strcpy(new_client->ip, data.ip);
                    new_client->status = 0;
                    new_client->port_udp = data.port_udp;
                    new_client->sockfd = sockfd;
                    clients.clients[clients.counter++] = new_client;
                    snprintf(buffer, sizeof(buffer), "REG_OK");
                }
                pthread_mutex_unlock(&clients.lock);
            }else{
                printf("[SERVER] Utente %s già registrato\n", data.email);
                snprintf(buffer, sizeof(buffer), "Utente già registrato");
            }
            if(send(sockfd, buffer, strlen(buffer), 0) < 0){
                perror("send");
                exit(EXIT_FAILURE);
            }
        }else if(!strcmp(data.operation, "login")){
            if(login(f, data)){
                printf("[SERVER] login avvenuto con successo per %s\n", data.email);
                
                pthread_mutex_lock(&clients.lock);
                int found = 0;
                for(int i = 0; i < clients.counter; i++){
                    if(!strcmp(clients.clients[i]->email, data.email) && !strcmp(clients.clients[i]->password, data.password)){
                        found = 1;
                        clients.clients[i]->status = 1;
                        clients.clients[i]->port_udp = data.port_udp;
                        strcpy(clients.clients[i]->ip, data.ip);
                        clients.clients[i]->sockfd = sockfd;
                        td = clients.clients[i];
                        break;
                    }
                }

                //se trovato ma non presente nella lista temporanea degli online 
                if (!found && clients.counter < MAX_CLIENTS) {
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
                snprintf(buffer, sizeof(buffer), "LOG_OK");
            }else{
                printf("[SERVER] Credenziali non valide per %s\n", data.email);
                snprintf(buffer, sizeof(buffer), "Email o Password Errate");
            }

            if(send(sockfd, buffer, strlen(buffer), 0) < 0){
                perror("send");
                exit(EXIT_FAILURE);
            }
        }
        else if(!strcmp(data.operation, "messaggio")){
            printf("[SERVER] Richiesta di messaggio da parte di %s\n", data.email);

            pthread_mutex_lock(&clients.lock);
            int found = 0;
            for(int i = 0; i < clients.counter; i++){
                if(!strcmp(clients.clients[i]->email, data.online_user)){
                    found = 1;
                    if(clients.clients[i]->status == 1){
                       printf("[SERVER] Utente %s online invio le informazioni per il P2P (%s:%d)\n", data.online_user,  clients.clients[i]->ip, clients.clients[i]->port_udp);
                       snprintf(buffer, sizeof(buffer), "USER_FOUND %s %d", clients.clients[i]->ip, clients.clients[i]->port_udp);
                    }else{
                        printf("[SERVER] Utente %s trovato ma non ancora online\n", data.online_user);
                        snprintf(buffer, sizeof(buffer), "Utente non ancora online");
                    }
                    break;
                }
            }

            pthread_mutex_unlock(&clients.lock);

            if(!found){
                snprintf(buffer, sizeof(buffer), "Utente non trovato");
            }

            if(send(sockfd, buffer, strlen(buffer), 0) < 0){
                perror("send");
                exit(EXIT_FAILURE);
            }
        }else if(!strcmp(data.operation, "exit")){
            printf("Chiusura connessione\n");
            break;
        }
    }
    close(sockfd);
    pthread_exit(NULL);
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

    if(listen(sockfd, MAX_CLIENTS) < 0){
        perror("listen");
        exit(EXIT_FAILURE);
    }

    printf("[SERVER] in ascolto su %d\n", PORT);

    if((f = fopen("database.txt", "a+")) == NULL){
        perror("fopen");
        exit(EXIT_FAILURE);
    }

    while(1){
        client_info* client = malloc(sizeof(client_info));
        socklen_t len = sizeof(client->client_addr);

        if((client->sockfd = accept(sockfd, (struct sockaddr*)&client->client_addr, &len)) < 0){
            perror("accept");
            free(client);
            continue;
        }
        
        pthread_t tid;
        if(pthread_create(&tid, NULL, (void*)handle_data, client) != 0){
            perror("pthread_create");
            close(client->sockfd);
            free(client);
        }

        pthread_detach(tid);
    }

    fclose(f);
    pthread_mutex_destroy(&clients.lock);
    close(sockfd);
    return 0;
}