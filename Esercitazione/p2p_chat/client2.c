/* Versioni iniziali di prova

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

#define PORT 8080
#define BUFFER_SIZE 1024
#define MAX_SIZE 100

typedef enum { REGISTER='r', LOGIN='l' } operation_off;
typedef enum { MESSAGE='m', EXIT='e' } operation_on;

typedef struct {
    char email[MAX_SIZE];
    char password[MAX_SIZE];
    char useronline[MAX_SIZE];
    operation_off op_of;
    operation_on op_on;
    char msg[MAX_SIZE];
    char ip[INET_ADDRSTRLEN];
    int port;
    int status; // 0: offline, 1: online
} client_data;

int sockfd;       // TCP verso server
int sockfd_udp;   // UDP per P2P
char user_on[MAX_SIZE];

// Thread per ricezione messaggi P2P
void p2p_listener(void* arg){
    client_data* td = (client_data*)arg;
    struct sockaddr_in sender_addr;
    socklen_t addrlen = sizeof(sender_addr);
    char buffer[BUFFER_SIZE];
    char sender[MAX_SIZE], message[MAX_SIZE];
    
    printf("[P2P] In ascolto su %s:%d\n", td->ip, td->port);

    while(1){
        ssize_t n = recvfrom(sockfd_udp, buffer, sizeof(buffer)-1, 0, (struct sockaddr*)&sender_addr, &addrlen);
        if(n > 0){
            buffer[n] = '\0';
            
            if(sscanf(buffer, "%[^:]:%[^\n]", sender, message) == 2){
                printf("\n[Messaggio P2P ricevuto da %s] %s\n", sender, message);
            } else {
                printf("\n[Messaggio P2P ricevuto] %s\n", buffer);
            }
        }
    }
}

// Funzione invio messaggi P2P
void send_p2p(const char* ip, int port, const char* msg){
    struct sockaddr_in p2p_addr;
    char buffer[BUFFER_SIZE];
    
    snprintf(buffer, sizeof(buffer), "%s : %s", user_on, msg);  
    memset(&p2p_addr, 0, sizeof(p2p_addr));
    p2p_addr.sin_family = AF_INET;
    p2p_addr.sin_port = htons(port);
    if(inet_pton(AF_INET, ip, &p2p_addr.sin_addr) <= 0){
        perror("inet_pton");
        return;
    }

    if(sendto(sockfd_udp, buffer, strlen(buffer), 0, (struct sockaddr*)&p2p_addr, sizeof(p2p_addr)) < 0){
        perror("sendto");
    }
}

// Thread per invio dati al server
void send_data(void* arg){
    client_data* td = (client_data*)arg;
    char input[MAX_SIZE];

    while(1){
        if(td->status == 0){
            printf("Operazione da eseguire (r: registrazione, l: login): ");
            if(!fgets(input, sizeof(input), stdin)) continue;
            td->op_of = input[0];

            switch(td->op_of){
                case REGISTER:
                    printf("Email: ");
                    fgets(td->email, MAX_SIZE, stdin); 
                    td->email[strcspn(td->email,"\n")]=0;
                    printf("Password: ");
                    fgets(td->password, MAX_SIZE, stdin); 
                    td->password[strcspn(td->password,"\n")]=0;
                    break;
                case LOGIN:
                    printf("Email: ");
                    fgets(td->email, MAX_SIZE, stdin); 
                    td->email[strcspn(td->email,"\n")]=0;
                    printf("Password: ");
                    fgets(td->password, MAX_SIZE, stdin); 
                    td->password[strcspn(td->password,"\n")]=0;
                    break;
                default:
                    printf("Operazione non valida.\n");
                    continue;
            }

            if(send(sockfd, td, sizeof(client_data), 0)<0){
                perror("Errore invio dati al server");
                exit(EXIT_FAILURE);
            }

        } else {
            printf("Operazione da eseguire (m: messaggio, e: esci): ");
            if(!fgets(input, sizeof(input), stdin)) continue;
            td->op_on = input[0];

            switch(td->op_on){
                case MESSAGE:
                    printf("Utente destinatario: ");
                    fgets(td->useronline, MAX_SIZE, stdin);
                    td->useronline[strcspn(td->useronline,"\n")]=0;
                    printf("Messaggio: ");
                    fgets(td->msg, MAX_SIZE, stdin); 
                    td->msg[strcspn(td->msg,"\n")]=0;

                    // invieo richiesta al server pr ottenere IP e porta
                    if(send(sockfd, td, sizeof(client_data), 0)<0){
                        perror("Errore invio richiesta al server");
                    }
                    break;

                case EXIT:
                    td->status = 0;
                    printf("Uscita...\n");
                    close(sockfd);
                    close(sockfd_udp);
                    exit(EXIT_SUCCESS);

                default:
                    printf("Operazione non valida.\n");
                    continue;
            }
        }
    }
}

// Thread per ricezione messaggi dal server
void recive_handle(void* arg){
    client_data* td = (client_data*)arg;
    char buffer[BUFFER_SIZE];

    while(1){
        ssize_t n = recv(sockfd, buffer, sizeof(buffer)-1, 0);
        if(n <= 0){
            perror("recv");
            exit(EXIT_FAILURE);
        }
        buffer[n] = '\0';

        if (!strcmp(buffer, "login_success")) {
            td->status = 1;
            printf("\nLogin riuscito! Utente online.\n");
        } else if(!strcmp(buffer, "login_failed")){
            td->status = 0;
            printf("\nLogin Fallito\n");
        } else if (!strcmp(buffer, "register_success")) {
            printf("\nRegistrazione riuscita!\n");
        } else if (!strcmp(buffer, "user_not_found")) {
            printf("\nUtente non trovato o offline!\n");
        } else if (!strncmp(buffer, "user_online", 11)) {
            char ip[INET_ADDRSTRLEN];
            int port;
            if(sscanf(buffer, "user_online %s %s %d", user_on, ip, &port) == 3){
                printf("\nUtente %s trovato online: %s:%d\n", user_on, ip, port);
                send_p2p(ip, port, td->msg);
            }
        } else if (!strcmp(buffer, "exit")) {
            printf("Chiusura del client...\n");
            close(sockfd);
            close(sockfd_udp);
            exit(EXIT_SUCCESS);
        }
    }
}

int main(int argc, char* argv[]){
    if(argc != 4){
        fprintf(stderr, "Usage: %s <IP_Client> <Port_P2P> <IP_server>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    client_data td;
    td.status = 0; // inizialmente offline
    td.port = atoi(argv[2]);
    strncpy(td.ip, argv[1], INET_ADDRSTRLEN);

    // Socket TCP verso server
    struct sockaddr_in server_addr;
    if((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0){
        perror("socket TCP");
        exit(EXIT_FAILURE);
    }
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(PORT);
    if(inet_pton(AF_INET, argv[3], &server_addr.sin_addr) <= 0){
        perror("inet_pton");
        exit(EXIT_FAILURE);
    }
    if(connect(sockfd, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0){
        perror("connect");
        exit(EXIT_FAILURE);
    }
    printf("Connesso al server %s:%d\n", argv[3], PORT);
    printf("Utente offline\n");

    // Socket UDP per P2P
    if((sockfd_udp = socket(AF_INET, SOCK_DGRAM, 0)) < 0){
        perror("socket UDP");
        exit(EXIT_FAILURE);
    }
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(td.port);
    if(inet_pton(AF_INET, td.ip, &addr.sin_addr) <= 0){
        perror("inet_pton");
        exit(EXIT_FAILURE);
    }
    if(bind(sockfd_udp, (struct sockaddr*)&addr, sizeof(addr)) < 0){
        perror("bind UDP");
        exit(EXIT_FAILURE);
    }

    pthread_t send_thread, recv_thread, p2p_thread;
    if(pthread_create(&p2p_thread, NULL, (void*)p2p_listener, &td)!=0){ 
        perror("pthread_create p2p"); 
        exit(EXIT_FAILURE);
    }
    if(pthread_create(&send_thread, NULL, (void*)send_data, &td)!=0){ 
        perror("pthread_create send"); 
        exit(EXIT_FAILURE);
    }
    if(pthread_create(&recv_thread, NULL, (void*)recive_handle, &td)!=0){ 
        perror("pthread_create recv"); 
        exit(EXIT_FAILURE);
    }

    pthread_join(send_thread, NULL);
    pthread_join(recv_thread, NULL);
    pthread_join(p2p_thread, NULL);

    close(sockfd);
    close(sockfd_udp);
}
