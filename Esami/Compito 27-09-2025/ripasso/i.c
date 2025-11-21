/*
Realizzare un sistema di messaggistica i e C (I e C fanno parte dello stesso nodo)
interprete.c:
- invia un messaggio a C per identificarsi (per presentarsi)
-  quando deve comunicare con qualcuno le richieste devono passare a C (C fa parte dello stesso nodo)
-  riceve i messaggi da C che arrivano da altri nodi 
- quando deve comunicare con qualche altro nodo passa ID, IP, porta (già a conoscenza del nodo) a C
- la comunicazione con C deve essere UDP
configurazione.c
- riceve le richieste di I
- Li inoltra a indirizzi IP, porta e ID specificato dall'interprete.c
- riceverà i messaggi proveniente da nodi esterni e li inoltrerà all'interprete.c
Il sistema si basa sull'utilizzo di messaggi di ACK se dopo 3 secondi l'interprete non riceve alcuna risposta rinvierà il messaggio per 3 volte
se continua a non ricevere risposta può riprendere con le operazioni
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <pthread.h>
#include <sys/socket.h>

#define BUFFER_SIZE 1024
#define MAX_SIZE 100
#define PORT 6000

typedef struct{
    char interface[MAX_SIZE];
    char operation[MAX_SIZE];
    char ip[INET_ADDRSTRLEN];
    char msg[MAX_SIZE];
    int id;
    int external_id;
    int external_port;
    int seq;
}interprete_data;

int sockfd;
struct sockaddr_in configuration_addr;

void receive_handle(void* arg){
    char buffer[BUFFER_SIZE];
    struct sockaddr_in from_addr;
    socklen_t len = sizeof(from_addr);

    while(1){
        ssize_t n = recvfrom(sockfd, buffer, BUFFER_SIZE - 1, 0, (struct sockaddr*)&from_addr, &len);
        if(n > 0){
            buffer[n] = '\0';
            printf("\nRicevuto: %s\n", buffer);
            fflush(stdout);
        }
    }

}

void handle_data(void* arg){
    interprete_data* td = (interprete_data*)arg;
    char buffer[BUFFER_SIZE];
    char id[MAX_SIZE];
    char port[MAX_SIZE];
    int seq_counter = 1;

    while(1){
        printf("Inserisci Operazioni, messaggio, exit: ");

        if(!fgets(buffer, BUFFER_SIZE, stdin)) continue;
        buffer[strcspn(buffer, "\n")] = '\0';

        if(!strcmp(buffer, "messaggio")){
            strcpy(td->operation, buffer);
            printf("ID destinatario: ");
            fgets(id, MAX_SIZE, stdin);
            id[strcspn(id, "\n")] = '\0';
            td->external_id = atoi(id);

            printf("IP destinatario: ");
            fgets(td->ip, INET_ADDRSTRLEN, stdin);
            td->ip[strcspn(td->ip, "\n")] = '\0';

            printf("Porta destinazione: ");
            fgets(port, MAX_SIZE, stdin);
            port[strcspn(port, "\n")] = '\0';
            td->external_port = atoi(port);

            printf("Messaggio: ");
            fgets(td->msg, MAX_SIZE, stdin);
            td->msg[strcspn(td->msg, "\n")] = '\0';

            td->seq = seq_counter++;
            int tentativi = 0;
            int ack = 0;

            while(tentativi < 3 && !ack){
                if(sendto(sockfd, td, sizeof(interprete_data), 0, (struct sockaddr*)&configuration_addr, sizeof(configuration_addr)) < 0){
                    perror("sendto");
                    break;
                }

                printf("[%s] Invio messaggio (tentaivo = %d, seq = %d)\n", td->interface, tentativi, td->seq);
                fd_set fds;
                FD_ZERO(&fds);
                FD_SET(sockfd, &fds);
                struct timeval tv = {.tv_sec = 3, .tv_usec = 0};

                int rv = select(sockfd + 1, &fds, NULL, NULL, &tv);
                if(rv == 0){
                    printf("Timeout\n");
                    tentativi++;
                    continue;
                }else if(rv < 0){
                    perror("select");
                    break;
                }
                else if(FD_ISSET(sockfd, &fds)){
                    char recv_buffer[BUFFER_SIZE];
                    struct sockaddr_in ack_addr;
                    socklen_t len = sizeof(ack_addr);

                    ssize_t n = recvfrom(sockfd, recv_buffer, BUFFER_SIZE -1, 0, (struct sockaddr*)&ack_addr, &len);
                    
                    if( n > 0){
                        recv_buffer[n] = '\0';

                        char expected_ack[BUFFER_SIZE];
                        snprintf(expected_ack, sizeof(expected_ack), "ACK_%d", td->seq);
                        if (!strcmp(recv_buffer, expected_ack)) {
                            printf("[%s] Ricevuto ACK di conferma '%s'\n", td->interface, expected_ack);
                            ack = 1;
                        }else {
                            printf("\nRicevuto: %s\n", recv_buffer);
                        }
                    }
                }
            }
            if(!ack){
                printf("Nessun ACK dopo 3 tentativi per la seq=%d\n", td->seq);
            }
        }
        else if(!strcmp(buffer, "exit")){
            strcpy(td->operation, buffer);
            if(sendto(sockfd, td, sizeof(interprete_data), 0, (struct sockaddr*)&configuration_addr, sizeof(configuration_addr)) < 0){
                perror("sendto");
            }
            close(sockfd);
            exit(EXIT_SUCCESS);
        }
    }
}

int main(int argc, char* argv[]){
    if(argc != 2){
        fprintf(stderr, "Usage: %s <ID>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    interprete_data td;
    td.id = atoi(argv[1]);

    if((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0){
        perror("socket");
        exit(EXIT_FAILURE);
    }

    memset(&configuration_addr, 0, sizeof(configuration_addr));
    configuration_addr.sin_family = AF_INET;
    configuration_addr.sin_port = htons(PORT);
    
    if(inet_pton(AF_INET, "127.0.0.1", &configuration_addr.sin_addr) <= 0){
        perror("inet_pton");
        exit(EXIT_FAILURE);
    }

    printf("[I] connesso a 127.0.0.1:%d\n", PORT);

    printf("Inserisci Nome interprete: ");
    fgets(td.interface, MAX_SIZE, stdin);
    td.interface[strcspn(td.interface, "\n")] = '\0';

    if(sendto(sockfd, &td, sizeof(interprete_data), 0, (struct sockaddr*)&configuration_addr, sizeof(configuration_addr)) < 0){
        perror("sendto");
    }

    pthread_t recv_thread, send_thread;

    if(pthread_create(&recv_thread, NULL, (void*)receive_handle, &td) != 0){
        perror("pthread_create");
        exit(EXIT_FAILURE);
    }

    if(pthread_create(&send_thread, NULL, (void*)handle_data, &td) != 0){
        perror("pthread_create");
        exit(EXIT_FAILURE);
    }

    pthread_join(send_thread, NULL);
    pthread_join(recv_thread, NULL);

    close(sockfd);
}