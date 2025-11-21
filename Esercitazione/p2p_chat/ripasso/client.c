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

int sockfd;
int sockfd_udp;

void receive_p2p(void* arg){
    char buffer[BUFFER_SIZE];
    struct sockaddr_in from_addr;
    socklen_t len = sizeof(from_addr);

    while(1){
        ssize_t n = recvfrom(sockfd_udp, buffer, BUFFER_SIZE - 1, 0, (struct sockaddr*)&from_addr, &len);

        if(n > 0){
            buffer[n] = '\0';
            printf("\nRicevuto UDP: %s\n" ,buffer);
            fflush(stdout);
        }else{
            perror("recvfrom");
        }
    }
}

void send_udp(client_data* td, const char* ip,  int port_udp){
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
            printf("\nLogin avvenuta con successo\n");
            td->status = 1;
            fflush(stdout);
        }
        else if(!strncmp(buffer, "USER_ON", 7)){
            char ip[INET_ADDRSTRLEN];
            int port_udp;
            if(sscanf(buffer, "USER_ON %s %d", ip, &port_udp) == 2){
                printf("\nInvio '%s' all'utente %s tramite P2P (%s:%d)\n", td->msg, td->user_online, ip, port_udp);
                send_udp(td, ip, port_udp);
            }
        }else{
            printf("\nRicevuto: %s\n", buffer);
            fflush(stdout);
        }
    }
}

void handle_data(void* arg){
    client_data* td = (client_data*)arg;
    char buffer[BUFFER_SIZE];

    while(1){
        if(td->status == 0){
            printf("Operazione disponibili: registrazione, login\n");
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
        }else if(td->status == 1){
            printf("Operazioni diponibili: messaggio, exit\n");
            printf("Inserisci Operazione: ");

            if(!fgets(buffer, BUFFER_SIZE, stdin)) continue;
            buffer[strcspn(buffer, "\n")] = '\0';

            if(!strcmp(buffer, "messaggio")){
                strcpy(td->operation, buffer);
                printf("Utente a cui inviare il messaggio: ");
                fgets(td->user_online, MAX_SIZE, stdin);
                td->user_online[strcspn(td->user_online, "\n")] = '\0';

                printf("Messaggio: ");
                fgets(td->msg, MAX_SIZE, stdin);
                td->msg[strcspn(td->msg, "\n")] = '\0';
                
                if(send(sockfd, td, sizeof(client_data), 0) < 0){
                    perror("send");
                    exit(EXIT_FAILURE);
                }
            }
            else if(!strcmp(buffer, "exit")){
                printf("Chiusura connessione\n");
                strcpy(td->operation, buffer);
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
        fprintf(stderr, "Usage: %s <ip_client> <ip_server> <port_udp>\n", argv[0]);
        exit(EXIT_SUCCESS);
    }

    client_data data;
    strcpy(data.ip, argv[1]);
    data.port_udp = atoi(argv[3]);
    data.status = 0;

    if((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0){
        perror("socket");
        exit(EXIT_FAILURE);
    }

    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(PORT);
    
    if(inet_pton(AF_INET, argv[2], &server_addr.sin_addr) <= 0){
        perror("inet_pton");
        exit(EXIT_FAILURE);
    }

    if(connect(sockfd, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0){
        perror("connect");
        exit(EXIT_FAILURE);
    }

    printf("Connesso al server %s:%d\n", argv[2], PORT);

    if((sockfd_udp = socket(AF_INET, SOCK_DGRAM, 0)) < 0){
        perror("socket");
        exit(EXIT_FAILURE);
    }

    struct sockaddr_in p2p_addr;
    memset(&p2p_addr, 0, sizeof(p2p_addr));
    p2p_addr.sin_family = AF_INET;
    p2p_addr.sin_port = htons(atoi(argv[3]));

    if(inet_pton(AF_INET, argv[1], &p2p_addr.sin_addr) <= 0){
        perror("inet_pton");
        exit(EXIT_FAILURE);
    }

    if(bind(sockfd_udp, (struct sockaddr*)&p2p_addr, sizeof(p2p_addr)) < 0){
        perror("bind");
        exit(EXIT_FAILURE);
    }

    printf("P2P in ascolto su %s:%s\n", argv[1], argv[3]);

    pthread_t p2p_thread, recv_thread, send_thread;

    if(pthread_create(&p2p_thread, NULL, (void*)receive_p2p, NULL) != 0){
        perror("pthread_create");
        exit(EXIT_FAILURE);
    }

    if(pthread_create(&recv_thread, NULL, (void*)receive_handle, &data) != 0){
        perror("pthread_create");
        exit(EXIT_FAILURE);
    }

    if(pthread_create(&send_thread, NULL, (void*)&handle_data, &data) != 0){
        perror("pthread_create");
        exit(EXIT_FAILURE);
    }

    pthread_join(p2p_thread, NULL);
    pthread_join(recv_thread, NULL);
    pthread_join(send_thread, NULL);

    close(sockfd_udp);
    close(sockfd);
}