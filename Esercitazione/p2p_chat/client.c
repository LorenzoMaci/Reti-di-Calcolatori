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

int sockfd;
int sockfd_udp;
pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;

void p2p_receive(void* arg){
    char buffer[BUFFER_SIZE];
    struct sockaddr_in from_addr;
    socklen_t len = sizeof(from_addr);

    while(1){
        ssize_t n = recvfrom(sockfd_udp, buffer, BUFFER_SIZE - 1, 0, (struct sockaddr*)&from_addr, &len);
        if(n > 0){
            buffer[n] = '\0';
            printf("\nRicevuto UDP: %s\n", buffer);
            fflush(stdout);
        }else{
            perror("recvfrom");
        }
    }
}

void send_p2p(client_data* td, const char* ip, int port_udp){
    char buffer[BUFFER_SIZE];
    snprintf(buffer, sizeof(buffer), "[%s]: %s", td->email, td->msg);

    struct sockaddr_in client_addr;
    memset(&client_addr, 0, sizeof(client_addr));
    client_addr.sin_family = AF_INET;
    client_addr.sin_port = htons(port_udp);
    
    if(inet_pton(AF_INET, ip, &client_addr.sin_addr) <= 0){
        perror("inet_pton");
        exit(EXIT_FAILURE);
    }

    if(sendto(sockfd_udp, buffer, strlen(buffer), 0, (struct sockaddr*)&client_addr, sizeof(client_addr)) < 0){
        perror("sendto");
    }
}

void receive_handle(void* arg){
    client_data* td = (client_data*)arg;
    char buffer[BUFFER_SIZE];

    while(1){
        ssize_t n = recv(sockfd, buffer, BUFFER_SIZE - 1, 0);
        if(n <= 0){
            if(n == 0){
                printf("Chiusura connessione\n");
            }
            perror("recv");
            exit(EXIT_FAILURE);
        }

        buffer[n] = '\0';

        if(!strcmp(buffer, "REG_OK")){
            printf("\nRegistrazione avvenuta con successo\n");
            fflush(stdout);
        }
        else if(!strcmp(buffer, "LOG_OK")){
            pthread_mutex_lock(&lock);
            td->status = 1;
            pthread_mutex_unlock(&lock);
            printf("\nLogin avvenuto con successo\n");
            fflush(stdout);
        }
        else if(!strncmp(buffer, "USER_FOUND", 10)){
            char ip[INET_ADDRSTRLEN];
            int port_udp;
            if((sscanf(buffer, "USER_FOUND %s %d", ip, &port_udp)) == 2){
                printf("Utente %s online invio il messaggio '%s' tramite P2P (%s:%d)\n", td->online_user, td->msg, ip, port_udp);
                send_p2p(td, ip, port_udp);
            }
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
        pthread_mutex_lock(&lock);
        int status = td->status;
        pthread_mutex_unlock(&lock);
        if(status == 0){
            printf("Operazioni disponibili, registrazione, login\n");
            printf("Inserisci Operazione: ");

            if(!fgets(buffer, BUFFER_SIZE, stdin)) continue;
            buffer[strcspn(buffer, "\n")] = '\0';

            if(!strcmp(buffer, "registrazione") || !strcmp(buffer, "login")){
                strcpy(td->operation, buffer);
                printf("Email: ");
                fgets(td->email, MAX_SIZE, stdin);
                td->email[strcspn(td->email, "\n")] = '\0';

                printf("Password: ");
                fgets(td->password, MAX_SIZE, stdin);
                td->password[strcspn(td->password, "\n")] = '\0';

                if(send(sockfd, td, sizeof(client_data), 0) < 0){
                    perror("send");
                    exit(EXIT_FAILURE);
                }
            }
        }
        else if(status == 1){
            printf("Operazioni disponibili, messaggio, exit\n");
            printf("Inserisci Operazione: ");

            if(!fgets(buffer, BUFFER_SIZE, stdin)) continue;
            buffer[strcspn(buffer, "\n")] = '\0';

            if(!strcmp(buffer, "messaggio")){
                strcpy(td->operation, buffer);
                printf("Utente a cui inviare il messaggio: ");
                fgets(td->online_user, MAX_SIZE, stdin);
                td->online_user[strcspn(td->online_user, "\n")] = '\0';

                printf("Messaggio: ");
                fgets(td->msg, MAX_SIZE, stdin);
                td->msg[strcspn(td->msg, "\n")] = '\0';

                if(send(sockfd, td, sizeof(client_data), 0) < 0){
                    perror("send");
                    exit(EXIT_FAILURE);
                }
            }
            else if(!strcmp(buffer, "exit")){
                strcpy(td->operation, buffer);
                printf("Chiusura connessione\n");
                if(send(sockfd, td, sizeof(client_data), 0) < 0){
                    perror("send");
                    exit(EXIT_FAILURE);
                }
                close(sockfd_udp);
                close(sockfd);
                exit(EXIT_SUCCESS);
            }
        }
    }
}

int main(int argc, char* argv[]){
    if(argc != 4){
        fprintf(stderr, "Usage: %s <ip_server> <ip_client> <port_udp>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    client_data td;
    td.status = 0;
    td.port_udp = atoi(argv[3]);
    strcpy(td.ip, argv[2]);
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

    if(connect(sockfd, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0){
        perror("connect");
        exit(EXIT_FAILURE);
    }

    printf("Connesso al server su %s:%d\n", argv[1], PORT);

    if((sockfd_udp = socket(AF_INET, SOCK_DGRAM, 0)) < 0){
        perror("socket");
        exit(EXIT_FAILURE);    
    }

    struct sockaddr_in p2p_addr;
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

    printf("Client in ascolto tramite P2P su %s:%s\n", argv[2], argv[3]);

    pthread_t p2p_thread, recv_thread, send_thread;

    if(pthread_create(&p2p_thread, NULL, (void*)p2p_receive, NULL) != 0){
        perror("pthread_create");
        exit(EXIT_FAILURE);
    }
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
    pthread_join(p2p_thread, NULL);

    close(sockfd_udp);
    close(sockfd);
}