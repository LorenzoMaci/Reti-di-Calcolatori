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

int sockfd;
int sockfd_udp;

void p2p_listener(void* arg){
    client_data* td = (client_data*)arg;
    struct sockaddr_in sender_addr;
    socklen_t len = sizeof(sender_addr);
    char buffer[BUFFER_SIZE];

    printf("[P2P] In ascolto su %s:%d\n", td->ip, td->port_udp);

    while(1){
        ssize_t n = recvfrom(sockfd_udp, buffer, sizeof(buffer) - 1, 0, (struct sockaddr*)&sender_addr, &len);
        if(n < 0){
            perror("recvfrom");
        }

        buffer[n] = '\0';

        printf("\nRicevuto: %s\n", buffer);
        fflush(stdout);
    }
}

void send_p2p(client_data* td, const char* ip, int port){
    struct sockaddr_in udp_addr;
    char buffer[BUFFER_SIZE];

    snprintf(buffer, sizeof(buffer), "[%s]: %s", td->email, td->msg);
    memset(&udp_addr, 0, sizeof(udp_addr));
    udp_addr.sin_family = AF_INET;
    udp_addr.sin_port = htons(port);

    if(inet_pton(AF_INET, ip, &udp_addr.sin_addr) <= 0){
        perror("inet_pton");
        exit(EXIT_FAILURE);
    }

    if(sendto(sockfd_udp, buffer, strlen(buffer), 0, (struct sockaddr*)&udp_addr, sizeof(udp_addr)) < 0){
        perror("sendto");
    }
}

void receive_handle(void* arg){
    client_data* td = (client_data*)arg;
    char buffer[BUFFER_SIZE];

    while(1){
        ssize_t n = recv(sockfd, buffer, BUFFER_SIZE-1, 0);
        if(n <= 0) {
            if(n == 0)
                printf("\nServer closed connection\n");
        else
            perror("Receive error");
            exit(1);
        }
        buffer[n] = '\0';

        if(!strcmp(buffer, "REG_OK")){
            printf("Registazione andata a buon fine\n");
            fflush(stdout);
            td->status = 0;
        }
        else if(!strcmp(buffer, "LOG_OK")){
            printf("\nLogin Effettuato\n");
            fflush(stdout);
            td->status = 1;
        }
        else if(!strncmp(buffer, "USER_ON", 7)){
            char ip[INET_ADDRSTRLEN];
            int port;
            if(sscanf(buffer, "USER_ON %s %d", ip, &port) == 2){
                printf("Utente online in ip: %s porta: %d\n", ip, port);
                send_p2p(td, ip, port);
            }
            fflush(stdout);
        }
        else if(!strcmp(buffer, "exit")){
            exit(EXIT_SUCCESS);
        }else{
            printf("\nReceived: %s\n> ", buffer);
            fflush(stdout);
        }
    }
}

void handle_client(void* arg){
    client_data* td = (client_data*)arg;

    while(1){
        char buffer[MAX_SIZE];
        if(td->status == 0){
            printf("Operazioni disponibili, registrazione, login: ");
            fflush(stdout);
            
            if(!fgets(buffer, MAX_SIZE, stdin)) continue;

            buffer[strcspn(buffer, "\n")] = '\0';

            if(!strcmp(buffer, "registrazione")){
                printf("Email: ");
                fgets(td->email, MAX_SIZE, stdin);
                td->email[strcspn(td->email, "\n")] = '\0';
                printf("Password: ");
                fgets(td->password, MAX_SIZE, stdin);
                td->password[strcspn(td->password, "\n")] = '\0';
                fflush(stdout);
                strcpy(td->operation, buffer);
            }
            else if(!strcmp(buffer, "login")){
                printf("Email: ");
                fgets(td->email, MAX_SIZE, stdin);
                td->email[strcspn(td->email, "\n")] = '\0';
                printf("Password: ");
                fgets(td->password, MAX_SIZE, stdin);
                td->password[strcspn(td->password, "\n")] = '\0';
                fflush(stdout);
                strcpy(td->operation, buffer);
            }
        }
        else if(td->status == 1){
            printf("Operazioni dispoinibili, messaggio, exit: ");
            fflush(stdout);
            
            if(!fgets(buffer, MAX_SIZE, stdin)) continue;

            buffer[strcspn(buffer, "\n")] = '\0';

            if(!strcmp(buffer, "messaggio")){
                printf("Utente a cui inviare il messaggio: ");
                fgets(td->useronline, MAX_SIZE, stdin);
                td->useronline[strcspn(td->useronline, "\n")] = '\0';
                printf("Messaggio: ");
                fgets(td->msg, MAX_SIZE, stdin);
                td->msg[strcspn(td->msg, "\n")] = '\0';
                fflush(stdout);
                strcpy(td->operation, buffer);
            }
            else if(!strcmp(buffer, "exit")){
                printf("Disconessione\n");
                strcpy(td->operation, buffer);
            }
        }

        if(send(sockfd, td, sizeof(client_data), 0) < 0){
            perror("send");
            exit(EXIT_FAILURE);
        }
    }
}

int main(int argc, char* argv[]){
    if(argc != 4){
        fprintf(stderr, "Usage: %s <ip_server> <ip_client> <port>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    client_data data;
    data.status = 0;
    data.port_udp = atoi(argv[3]);
    strncpy(data.ip, argv[2], INET_ADDRSTRLEN);

    struct sockaddr_in server_addr, p2p_addr;

    if((sockfd = socket(AF_INET, SOCK_STREAM, 0 )) < 0){
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

    if(connect(sockfd, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0){
        perror("connect");
        exit(EXIT_FAILURE);
    }

    printf("[CLIENT] connesso al server su %s:%d\n", argv[1], PORT);

    if((sockfd_udp = socket(AF_INET, SOCK_DGRAM, 0)) < 0){
        perror("socket");
        exit(EXIT_FAILURE);
    }

    memset(&p2p_addr, 0, sizeof(p2p_addr));
    p2p_addr.sin_family = AF_INET;
    p2p_addr.sin_port = htons(atoi(argv[3]));
 
    if(inet_pton(AF_INET, argv[2], &p2p_addr.sin_addr) <= 0){
        perror("inet_pton");
        exit(EXIT_FAILURE);
    }

    if(bind(sockfd_udp, (struct sockaddr*)&p2p_addr, sizeof(p2p_addr)) < 0){
        perror("bind");
        exit(EXIT_FAILURE);
    }

    pthread_t send_thread, recv_thread, p2p_thread;

    if(pthread_create(&recv_thread, NULL, (void*)receive_handle, &data) != 0){
        perror("pthread_create");
        exit(EXIT_FAILURE);
    }

    if(pthread_create(&p2p_thread, NULL, (void*)p2p_listener, &data) != 0){
        perror("pthread_create");
        exit(EXIT_FAILURE);
    }

    if(pthread_create(&send_thread, NULL, (void*)handle_client, &data) != 0){
        perror("pthread_create");
        exit(EXIT_FAILURE);
    }
    pthread_join(send_thread, NULL);
    pthread_join(recv_thread, NULL);
    pthread_join(p2p_thread, NULL);

    close(sockfd_udp);
    close(sockfd);
}