/*Versioni iniziali di prova
Creare un servizio di chat in cui gli utenti possano registrarsi e accedere al servizio di chat e comunicare con 
utenti specifici tramite comunicazione P2P.
Client:
Possono registrarsi utilizzando e-mail e password
Possono inviare un messaggio a un utente specifico (se connesso)
Per comunicare con altri utenti, il client deve utilizzare una comunicazione non affidabile
Per fare ciò, i client pubblicheranno sul server la propria porta di comunicazione speciale

Server principale:
Gestire la registrazione e l'autenticazione
Le informazioni degli utenti devono essere salvate in un database (un file.txt)
All'avvio, il servizio deve ricaricare il database (il file)
Il server scambia le informazioni dei client per consentire loro di comunicare in modalità P2P, senza passare attraverso il server
*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>

#define PORT 8080
#define BUFFER_SIZE 1024
#define MAX_SIZE 100    
#define MAX_CLIENTS 50

typedef enum{REGISTER = 'r', LOGIN = 'l'}operation_off;
typedef enum{MESSAGE = 'm', EXIT = 'e'}operation_on;

typedef struct{
    char email[MAX_SIZE];
    char password[MAX_SIZE];
    char useronline[MAX_SIZE]; // destinatario
    operation_off op_of;
    operation_on op_on;
    char msg[MAX_SIZE];
    char ip[INET_ADDRSTRLEN];
    int port;
    int status; // 0: offline, 1: online
}client_data;

typedef struct{
    int sockfd;
    char email[MAX_SIZE];
    char ip[INET_ADDRSTRLEN];
    int port;
    struct sockaddr_in addr;
}client_info;

typedef struct{
    client_info* clients[MAX_CLIENTS];
    int counter;
    pthread_mutex_t lock;
}client_list;

client_list clients = {.counter = 0, .lock = PTHREAD_MUTEX_INITIALIZER};

int login_user(client_data* td, FILE* fb){
    rewind(fb);
    char buffer[BUFFER_SIZE];
    char email[MAX_SIZE], password[MAX_SIZE], ip[INET_ADDRSTRLEN];
    int port;
    while(fgets(buffer, BUFFER_SIZE, fb)){
        if(sscanf(buffer, "%s %s %s %d", email, password, ip, &port) == 4){
            if(!strcmp(td->email, email) && !strcmp(td->password, password)){
                return 1;
            }
        }
    }
    return 0;
}

int reg(client_data* td, FILE* fb){
    if(login_user(td, fb)){
        return 0; // Utente già esistente
    }
    fseek(fb, 0, SEEK_END);
    fprintf(fb, "%s %s %s %d\n", td->email, td->password, td->ip, td->port);
    fflush(fb);
    return 1;
}

void handle_client(void* arg){
    client_info* td = (client_info*)arg;
    int sockfd = td->sockfd;
    client_data client;
    char msg[BUFFER_SIZE];
    FILE* fb;

    printf("[SERVER] Nuova connessione TCP avviata\n");

    while(1){
        ssize_t n = recv(sockfd, &client, sizeof(client_data), 0);
        if(n <= 0){
            printf("[SERVER] Connessione chiusa da %s:%d\n", inet_ntoa(td->addr.sin_addr), ntohs(td->addr.sin_port));
            close(sockfd);
            free(td);
            pthread_exit(NULL);
        }

        if((fb = fopen("database.txt", "a+")) == NULL){
            perror("fopen");
            exit(EXIT_FAILURE);
        }

        if(client.op_of == REGISTER){
            if(reg(&client, fb)){
                snprintf(msg, BUFFER_SIZE, "register_success");
                printf("[SERVER] Registrazione riuscita per il Client: %s (P2P %s:%d)\n", client.email, client.ip, client.port);
            } else {
                snprintf(msg, BUFFER_SIZE, "register_failed");
                printf("[SERVER] Registrazione fallita per il Client: %s\n", client.email);
            }
            client.status = 0; // offline
            send(sockfd, msg, strlen(msg)+1, 0);

        } else if(client.op_of == LOGIN){
            if(login_user(&client, fb)){
                pthread_mutex_lock(&clients.lock);

                int found = 0;
                for(int i = 0; i < clients.counter; i++){
                    if(!strcmp(clients.clients[i]->email, client.email)){
                        strcpy(clients.clients[i]->ip, client.ip);
                        clients.clients[i]->port = client.port;
                        found = 1;
                        break;
                    }
                }

                if(!found && clients.counter < MAX_CLIENTS){
                    client_info* new_client = malloc(sizeof(client_info));
                    strcpy(new_client->email, client.email);
                    strcpy(new_client->ip, client.ip);
                    new_client->port = client.port;
                    new_client->sockfd = sockfd;
                    clients.clients[clients.counter++] = new_client;
                }

                pthread_mutex_unlock(&clients.lock);

                snprintf(msg, BUFFER_SIZE, "login_success");
                printf("[SERVER] Login riuscito per: %s (UDP %s:%d)\n", client.email, client.ip, client.port);
                client.status = 1; // online
            } else {
                snprintf(msg, BUFFER_SIZE, "login_failed");
                client.status = 0;
                printf("[SERVER] Login FALLITO: %s\n", client.email);
            }
            send(sockfd, msg, strlen(msg)+1, 0);
        } else if(client.op_on == MESSAGE){
            pthread_mutex_lock(&clients.lock);
            int found = 0;
            for(int i = 0; i < clients.counter; i++){
                if(!strcmp(clients.clients[i]->email, client.useronline)){ 
                    snprintf(msg, BUFFER_SIZE, "user_online %s %s %d", clients.clients[i]->email, clients.clients[i]->ip, clients.clients[i]->port);
                    found = 1;
                    break;
                }
            }
            pthread_mutex_unlock(&clients.lock);

            if(!found){
                snprintf(msg, BUFFER_SIZE, "user_not_found");
            }
            send(sockfd, msg, strlen(msg)+1, 0);

        } else if(client.op_on == EXIT){
            printf("[SERVER] Utente %s ha chiuso la connessione.\n", client.email);
            close(sockfd);
            free(td);
            pthread_exit(NULL);
        }

        fclose(fb);
    }
}

int main(int argc, char* argv[]){
    if(argc != 2){
        fprintf(stderr, "Usage: %s <IP_Server>\n", argv[0]);
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
    server_addr.sin_port = htons(PORT);
    
    if(inet_pton(AF_INET, argv[1], &server_addr.sin_addr) <= 0){
        perror("inet_pton");
        exit(EXIT_FAILURE);
    }

    if(bind(sockfd, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0){
        perror("bind");
        exit(EXIT_FAILURE);
    }

    if(listen(sockfd, 5) < 0){
        perror("listen");
        exit(EXIT_FAILURE);
    }

    printf("Server in ascolto su %s:%d\n", argv[1], PORT);

    while(1){
        client_info* td = malloc(sizeof(client_info));
        socklen_t addr_len = sizeof(td->addr);
        int new_sockfd = accept(sockfd, (struct sockaddr*)&td->addr, &addr_len);
        if(new_sockfd < 0){
            perror("accept");
            free(td);
            continue;
        }

        td->sockfd = new_sockfd;
        pthread_t thread_id;
        if(pthread_create(&thread_id, NULL, (void*)handle_client, td) != 0){
            perror("pthread_create");
            close(new_sockfd);
            free(td);
            continue;
        }
        pthread_detach(thread_id);
    }
    close(sockfd);
}
