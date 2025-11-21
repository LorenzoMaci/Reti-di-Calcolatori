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
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <pthread.h>

#define BUFFER_SIZE 1024
#define MAX_SIZE 100
#define PORT 8080

typedef struct{
    char email[MAX_SIZE];
    char password[MAX_SIZE];
    char msg[MAX_SIZE];
    char useron[MAX_SIZE];
    char operation[MAX_SIZE];
    int status; // 0 = off, 1 = on
}client_data;

int sockfd;

void receive_handle(void* arg){
    client_data* td = (client_data*)arg;
    char buffer[BUFFER_SIZE];

    while(1){
        ssize_t n = recv(sockfd, buffer, BUFFER_SIZE - 1, 0);
        if(n <= 0){
            if(n == 0){
                printf("Connessione chiusa dal server\n");
            }
            perror("recv");
            exit(EXIT_FAILURE);
        }

        buffer[n] = '\0';

        if(!strcmp(buffer, "REG_OK")){
            printf("\nRegistrazione avvenuta con successo\n");
            td->status = 0;
        }
        else if(!strcmp(buffer, "LOG_OK")){
            printf("\nLogin avvenuta con successo\n");
            td->status = 1;
            printf("Sei ora Online\n");
        }
        else if(!strcmp(buffer, "MSG_OK")){
            printf("\nMessaggio inviato\n");
        }
        else if(!strcmp(buffer, "not_online")){
            printf("\nUtente non online ma ha ricevuto il messaggio\n");
        }
        else if(!strcmp(buffer, "exit")){
            printf("\nConnessione chiusa dal Server\n");
            exit(EXIT_SUCCESS);
        }else{
            printf("\nReceived: %s\n", buffer);
        }
    }
}

void handle_client(void* arg){
    client_data* td = (client_data*)arg;

    while(1){
        char op[BUFFER_SIZE];
        if(td->status == 0){
            printf("Operazioni disponibili: registrazione, login: ");
            if(!fgets(op, BUFFER_SIZE, stdin)) continue;

            op[strcspn(op, "\n")] = '\0';

            if(!strcmp(op, "registrazione")){
                strcpy(td->operation, "registrazione");
                printf("Inserisci email: ");
                fgets(td->email, MAX_SIZE,  stdin);
                td->email[strcspn(td->email, "\n")] = '\0';
                printf("Inserisci password: ");
                fgets(td->password, MAX_SIZE, stdin);
                td->password[strcspn(td->password, "\n")] = '\0';
            }else if(!strcmp(op, "login")){
                strcpy(td->operation, "login");
                printf("Inserisci email: ");
                fgets(td->email, MAX_SIZE,  stdin);
                td->email[strcspn(td->email, "\n")] = '\0';
                printf("Inserisci password: ");
                fgets(td->password, MAX_SIZE, stdin);
                td->password[strcspn(td->password, "\n")] = '\0';
            }

            if(send(sockfd, td, sizeof(client_data), 0) < 0){
                perror("send");
                exit(EXIT_FAILURE);
            }
        }else if(td->status == 1){
            printf("Operazioni disponibili: messaggio, exit: ");
            if(!fgets(op, BUFFER_SIZE, stdin)) continue;

            op[strcspn(op, "\n")] = '\0';

            if(!strcmp(op, "exit")){
                strcpy(td->operation, op);
                if(send(sockfd, td, sizeof(client_data), 0) < 0){
                    perror("send");
                    exit(EXIT_FAILURE);
                }
            }
            else if(!strcmp(op, "messaggio")){
                strcpy(td->operation, "messaggio");
                printf("Utente a cui inviare il messaggio: ");
                fgets(td->useron, MAX_SIZE, stdin);
                td->useron[strcspn(td->useron, "\n")] = '\0';
                printf("Messaggio: ");
                fgets(td->msg, MAX_SIZE, stdin);
                td->msg[strcspn(td->msg, "\n")] = '\0'; 
                if(send(sockfd, td, sizeof(client_data), 0) < 0){
                    perror("send");
                    exit(EXIT_FAILURE);
                }
            }
        }
    }
}

int main(int argc, char* argv[]){
    if(argc != 2){
        fprintf(stderr, "Usage: %s <ip_server>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    client_data td;
    td.status = 0;
    struct sockaddr_in server_addr;

    if((sockfd = socket(AF_INET, SOCK_STREAM , 0)) < 0){
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
    
    printf("Connected to %s:%d\n", argv[1], PORT);

    pthread_t recv_thread, send_thread;
    if(pthread_create(&recv_thread, NULL, (void*)receive_handle, &td) != 0){
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