/*
Creare un servizio di chat in cui gli utenti possano registrarsi e accedere al servizio di chat 
e comunicare con utenti online e 
Client: 
- Possono registrarsi utilizzando e-mail e password 
- Possono inviare un messaggio a un utente specifico (se connesso) 
- La comunicazione tra client e server deve essere TCP
- Nel caso in cui l'utente non è connesso il client potrà inviare i messaggi e il server invierà i messaggi non appena l'utente si connetterà 
Server principale: 
- Gestire la registrazione e l'autenticazione (NON TRAMITE FILE IL SERVER NON APPENA SI CHIUDE NON RICORDA PIU' NULLA)
- Gestire l'invio corretto del messaggio tra client A e client B
- Nel caso in cui l'utente sia offline salvare 10 messaggi dal più recente al meno recente e appena l'utente si connetterà verranno inviati 
- dovrà inviare le operazioni effettuate al server logging, in una comunicazione UDP conoscendo solo la porta, il server principale non è a conoscenza dell'indirizzo IP 

Server logging 
- scriverà le operazioni che vengono effettuate sul terminale.

La comunicazione tra client e server principale deve essere TCP (bidirezionale), la connessione tra server principale e server logging è UDP (unidirezionale)
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
#define PORT_RECV 8080
#define PORT 8081
#define MAX_CLIENT 15
#define OFFLINE_MAX 10

typedef struct{
    char email[MAX_SIZE];
    char password[MAX_SIZE];
    char msg[MAX_SIZE];
    char useron[MAX_SIZE];
    char operation[MAX_SIZE];
    int status; // 0 = off, 1 = on
}client_data;

typedef struct{
    int sockfd;
    struct sockaddr_in addr;
    char email[MAX_SIZE];
    char password[MAX_SIZE];
    char msg[MAX_SIZE];
    char useron[MAX_SIZE];
    char operation[MAX_SIZE];
    int status; // 0 = off, 1 = on
    char ip[INET_ADDRSTRLEN];

    // Messaggi offline
    char offline_msgs[OFFLINE_MAX][BUFFER_SIZE];
    int offline_count;
}client_info;

typedef struct{
    client_info* clients[MAX_CLIENT];
    int counter;
    pthread_mutex_t lock;
}client_list;

client_list clients = {.counter = 0, .lock = PTHREAD_MUTEX_INITIALIZER};

void send_logging(const char* msg){
    int sockfd;
    struct sockaddr_in logging_addr;

    if((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0){
        perror("socket");
        exit(EXIT_FAILURE);
    }

    int yes = 1;
    if(setsockopt(sockfd, SOL_SOCKET, SO_BROADCAST, &yes, sizeof(yes)) < 0){
        perror("setsockop");
        exit(EXIT_FAILURE);
    }

    memset(&logging_addr, 0, sizeof(logging_addr));
    logging_addr.sin_family = AF_INET;
    logging_addr.sin_port = htons(PORT);
    logging_addr.sin_addr.s_addr = htonl(INADDR_BROADCAST);

    if(sendto(sockfd, msg, strlen(msg), 0, (struct sockaddr*)&logging_addr, sizeof(logging_addr)) < 0){
        perror("setsockopt");
    }

    printf("[SERVER]: Messaggio broadcast inviato al Server Loggin sulla porta %d\n", PORT);
    close(sockfd);
}

void handle_client(void* arg){
    client_info* td = (client_info*)arg;
    int client_sock = td->sockfd;
    char buffer[BUFFER_SIZE];
    char msg[BUFFER_SIZE];
    char client_ip[INET_ADDRSTRLEN];

    inet_ntop(AF_INET, &td->addr.sin_addr, client_ip, INET_ADDRSTRLEN);
    int client_port = ntohs(td->addr.sin_port);
    printf("New connection from %s:%d\n", client_ip, client_port);

    while(1){
        client_data data;
        ssize_t n = recv(client_sock, &data, sizeof(client_data), 0);
        if(n <= 0){
            if(n == 0){
                printf("Connesione chiusa\n");
            }
            perror("recv");
           break;
        }

        
        if(!strcmp(data.operation, "registrazione")){
            printf("Registrazione richiesta\n");
            pthread_mutex_lock(&clients.lock);

            if(clients.counter < MAX_CLIENT){
                client_info* new_client = malloc(sizeof(client_info));
                strcpy(new_client->email, data.email);
                strcpy(new_client->password, data.password);
                new_client->status = 0;
                new_client->sockfd = client_sock;
                new_client->offline_count = 0;   
                strcpy(new_client->ip, client_ip);
                clients.clients[clients.counter++] = new_client;
            }
            pthread_mutex_unlock(&clients.lock);
            snprintf(buffer, sizeof(buffer), "REG_OK");
            if(send(client_sock, buffer, strlen(buffer), 0) < 0){
                perror("send");
                exit(EXIT_FAILURE);
            }
            
            send_logging(buffer);
        }
        else if(!strcmp(data.operation, "login")){
            printf("Login richiesto\n");
            pthread_mutex_lock(&clients.lock);
    
            for(int i = 0; i < clients.counter; i++){
                if(!strcmp(clients.clients[i]->email, data.email) && !strcmp(clients.clients[i]->password, data.password)){
                    clients.clients[i]->status = 1;
                    
                    //per i messaggi offline
                    clients.clients[i]->sockfd = client_sock;
                    for(int j = 0; j < clients.clients[i]->offline_count; j++){
                        if(send(client_sock, clients.clients[i]->offline_msgs[j], strlen(clients.clients[i]->offline_msgs[j]), 0) < 0){
                            perror("send");
                            exit(EXIT_FAILURE);
                        }
                    }
                    clients.clients[i]->offline_count = 0;
                    memset(clients.clients[i]->offline_msgs, 0, sizeof(clients.clients[i]->offline_msgs));
                }
            }

            pthread_mutex_unlock(&clients.lock);
            snprintf(buffer, sizeof(buffer), "LOG_OK");
            if(send(client_sock, buffer, strlen(buffer), 0) < 0){
                perror("send");
                exit(EXIT_FAILURE);
            }
            send_logging(buffer);
        }else if(!strcmp(data.operation, "messaggio")){
            pthread_mutex_lock(&clients.lock);
            int found = 0;
            for(int i = 0; i < clients.counter; i++){
                if(!strcmp(clients.clients[i]->email, data.useron)){
                    found = 1;
                    if(clients.clients[i]->status == 1){
                        printf("Utente Online invio il messaggio\n");
                        snprintf(buffer, sizeof(buffer), "[%s]: %s\n", data.email, data.msg);
                        if(send(clients.clients[i]->sockfd, buffer, strlen(buffer), 0) < 0){
                            perror("send");
                            exit(EXIT_FAILURE);
                        }
                    }else{
                        //parte di codice nel caso in cui l'utente è offline
                        if(clients.clients[i]->offline_count < OFFLINE_MAX){
                            snprintf(clients.clients[i]->offline_msgs[clients.clients[i]->offline_count++], BUFFER_SIZE, "[%s]: %s\n",data.email, data.msg);
                            printf("Messaggio salvato per %s (offline)\n", clients.clients[i]->email);
                        }else{
                            printf("Casella messaggi piena per %s, messaggio scartato\n", clients.clients[i]->email);
                        }
                    }
                    break;
                }
            }
            pthread_mutex_unlock(&clients.lock);
            if(found){
                snprintf(msg, sizeof(msg), "MSG_OK");
                if(send(client_sock, msg, strlen(msg), 0) < 0){
                    perror("send");
                    exit(EXIT_FAILURE);
                }
                send_logging(msg);     
            }else{
                snprintf(msg, sizeof(msg), "MSG_FAIL");
                if(send(client_sock, msg, strlen(msg), 0) < 0){
                    perror("send");
                    exit(EXIT_FAILURE);
                }
                send_logging(msg);     
            }
        }else if(!strcmp(data.operation, "exit")){
            snprintf(buffer, sizeof(buffer), "exit");
            if(send(client_sock, buffer, strlen(buffer), 0) < 0){
                perror("send");
                exit(EXIT_FAILURE);
            }
            send_logging(buffer);
            break;
        }
    }
    // remove client from list
    pthread_mutex_lock(&clients.lock);

    for (int i = 0; i < clients.counter; i++) {
        if (clients.clients[i]->sockfd == client_sock) {
            free(clients.clients[i]);   // Libero la memoria associata al client
            // Shift a sinistra tutti i successivi
            for (int j = i; j < clients.counter - 1; j++) {
                clients.clients[j] = clients.clients[j + 1];
            }
            clients.counter--;
            break; // esco dal ciclo, client trovato e rimosso
        }
    }
    pthread_mutex_unlock(&clients.lock);
    close(client_sock);
}


int main(){
    int sockfd;
    struct sockaddr_in recv_addr;

    if((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0){
        perror("socket");
        exit(EXIT_FAILURE);
    }

    memset(&recv_addr, 0, sizeof(recv_addr));
    recv_addr.sin_family = AF_INET;
    recv_addr.sin_port = htons(PORT_RECV);
    recv_addr.sin_addr.s_addr = htonl(INADDR_ANY);

    if(bind(sockfd, (struct sockaddr*)&recv_addr, sizeof(recv_addr)) < 0){
        perror("bind");
        exit(EXIT_FAILURE);
    }

    if(listen(sockfd, MAX_CLIENT) < 0){
        perror("listen");
        exit(EXIT_FAILURE);
    }

    printf("[SERVER]: in ascolto sulla porta %d\n", PORT_RECV);

    while(1){
        client_info* client = malloc(sizeof(client_info));
        socklen_t len  = sizeof(client->addr);

        if((client->sockfd = accept(sockfd, (struct sockaddr*)&client->addr, &len)) < 0){
            perror("accept");
            free(client);
            continue;
        }

        pthread_t client_thread;
        if(pthread_create(&client_thread, NULL, (void*)handle_client, client) != 0){
            perror("pthread_Create");
            free(client);
            close(client->sockfd);
            exit(EXIT_FAILURE);
        }

        pthread_detach(client_thread);
    }
    close(sockfd);
    pthread_mutex_destroy(&clients.lock);
}