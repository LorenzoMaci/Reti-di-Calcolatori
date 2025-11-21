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

typedef struct{
    char email[MAX_SIZE];
    char password[MAX_SIZE];
    char operation[MAX_SIZE];
    char useronline[MAX_SIZE];
    char msg[MAX_SIZE];
    int status;
}client_data;

int sockfd;

void receive_handle(void* arg){
    client_data* td = (client_data*)arg;
    char buffer[BUFFER_SIZE];

    while(1){
        ssize_t n = recv(sockfd, buffer, BUFFER_SIZE - 1, 0);
        if(n <= 0){
            if(n == 0){
                printf("Connessione chiusa\n");
            }
            perror("recv");
            exit(EXIT_FAILURE);
        }

        buffer[n] = '\0';

        if(!strcmp(buffer, "REG_OK")){
            printf("\nRegistrazione andata a buon fine\n");
            fflush(stdout);
        }else if(!strcmp(buffer, "LOG_OK")){
            printf("\nLogin andato a buon fine\n");
            td->status = 1;
            printf("\nSei ora Online\n");
            fflush(stdout);
        }else if(!strcmp(buffer, "MSG_OK")){
            printf("\nMessaggio Inviato all'utente\n");
            fflush(stdout);
        }else if(!strcmp(buffer, "MSG_SAVE")){
            printf("\nMessagio in Coda utente non online\n");
            fflush(stdout);
        }else if(!strcmp(buffer, "exit")){
            printf("\nChiudo la connesione\n");
            exit(EXIT_SUCCESS);
        }else{
            printf("\nRicevuto: %s\n", buffer);
            fflush(stdout);
        }
    }
}

void handle_client(void* arg){
    client_data* td = (client_data*)arg;
    char buffer[BUFFER_SIZE];
    
    while(1){
        if(td->status == 0){
            printf("Operazioni disponibili: registrazione, login: ");
            fflush(stdout);

            if(!fgets(buffer, BUFFER_SIZE, stdin))continue;

            buffer[strcspn(buffer, "\n")] = '\0';
            if(!strcmp(buffer, "registrazione")){
                strcpy(td->operation, buffer);
                fgets(td->email, MAX_SIZE, stdin);
                td->email[strcspn(td->email, "\n")] = '\0';
                fgets(td->password, MAX_SIZE, stdin);
                td->password[strcspn(td->password, "\n")] = '\0';
                fflush(stdout);
            }
            if(!strcmp(buffer, "login")){
                strcpy(td->operation, buffer);
                printf("Email: ");
                fgets(td->email, MAX_SIZE, stdin);
                td->email[strcspn(td->email, "\n")] = '\0';
                printf("Password: ");
                fgets(td->password, MAX_SIZE, stdin);
                td->password[strcspn(td->password, "\n")] = '\0';
                fflush(stdout);
            }

            if(send(sockfd, td, sizeof(client_data), 0) < 0){
                perror("send");
                exit(EXIT_FAILURE);
            }
        }
        if(td->status == 1){
            printf("Operazioni disponibili online: messaggio, exit: ");
            fflush(stdout);

            if(!fgets(buffer, BUFFER_SIZE, stdin))continue;

            if(!strcmp(buffer, "messaggio")){
                strcpy(td->operation, buffer);
                printf("Utente a cui inviare il messaggio: ");
                fgets(td->useronline, MAX_SIZE, stdin);
                td->useronline[strcspn(td->useronline, "\n")] = '\0';
                printf("Messaggio: ");
                fgets(td->msg, MAX_SIZE, stdin);
                td->msg[strcspn(td->msg, "\n")] = '\0';
            }
            if(!strcmp(buffer, "exit")){
                printf("Chiusura connessione\n");
                strcpy(td->operation, buffer);
            }
            
            if(send(sockfd, td, sizoef(client_data), 0) < 0){
                perror("send");
                exit(EXIT_FAILURE);
            }
        }
    }
}

int main(int argc, char* argv[]){
    if(argc != 3){
        fprintf(stderr, "Usage: %s <ip_server> <port>(8080)\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    client_data td;
    td.status = 0;
    struct sockaddr_in server_addr;

    if((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0){
        perror("socket");
        exit(EXIT_FAILURE);
    }

    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(atoi(argv[2]));

    if(inet_pton(AF_INET, argv[1], &server_addr.sin_addr) <= 0){
        perror("inet_pton");
        exit(EXIT_FAILURE);
    }

    if(connect(sockfd, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0){
        perror("connect");
        exit(EXIT_FAILURE);
    }

    printf("Connesso a %s:%s\n", argv[1], argv[2]);
    
    pthread_t send_thread, recv_thread;
    if(pthread_cretate(&recv_thread, NULL, (void*)receive_handle, &td) != 0){
        perror("pthread_create");
        exit(EXIT_FAILURE);
    }

    if(pthread_create(&send_thread, NULL, (void*)handle_client, &td) != 0){
        perror("pthread_create");
        exit(EXIT_FAILURE);
    }

    pthread_join(send_thread, NULL);
    pthread_join(recv_thread, NULL);

    close(sockfd);
}