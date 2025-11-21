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

//Per farlo comunicare con un altro interprete copiare e incollare entrambi i codici Interprete e configurazione cambiando indirizzo IP e porte
#include <stdio.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>

#define BUFFER_SIZE 1024
#define PORT 6666
#define MAX_SIZE 100

typedef struct{
    char interprete[MAX_SIZE];
    int id_dest;
    int id;
    char ip[INET_ADDRSTRLEN];
    int port_dest;
    char msg[MAX_SIZE];
    char operation[MAX_SIZE];
}info_data;

int sockfd;
struct sockaddr_in control_addr;

void receive_handle(void* arg){
    char buffer[BUFFER_SIZE];
    struct sockaddr_in addr;
    socklen_t len = sizeof(addr);

    while(1){
        ssize_t n = recvfrom(sockfd, buffer, BUFFER_SIZE -1, 0, (struct sockaddr*)&addr, &len);
        if(n > 0){
            buffer[n] = '\0';
            printf("\nRicevuto: %s\n", buffer);
            fflush(stdout);
        }
    }
}

void handle_info(void* arg){
    info_data* td = (info_data*)arg;
    char buffer[BUFFER_SIZE];
    char id_dest[MAX_SIZE];
    char port_dest[MAX_SIZE];

    while(1){
        printf("Operazioni disponibili: messaggio, exit\n");
        printf("insersici operazione: ");

        if(!fgets(buffer, BUFFER_SIZE, stdin)) continue;
        buffer[strcspn(buffer, "\n")] = '\0';

        if(!strcmp(buffer, "messaggio")){
                strcpy(td->operation, buffer);
                printf("Inserisci ID: ");
                fgets(id_dest, MAX_SIZE, stdin);
                id_dest[strcspn(id_dest, "\n")] = '\0';
                td->id_dest = atoi(id_dest);
                
                printf("Inserisci IP destinatario: ");
                fgets(td->ip, INET_ADDRSTRLEN, stdin);
                td->ip[strcspn(td->ip, "\n")] = '\0';
                
                printf("Inserisci porta di destinazione: ");
                fgets(port_dest, MAX_SIZE, stdin);
                port_dest[strcspn(port_dest, "\n")] = '\0';
                td->port_dest = atoi(port_dest);
                
                printf("Inserisci messaggio da inviare: ");
                fgets(td->msg, MAX_SIZE, stdin);
                td->msg[strcspn(td->msg, "\n")] = '\0';
        }else if(!strcmp(buffer, "exit")){
            printf("Chiusura Connessione\n");
            strcpy(td->operation, buffer);
            if(sendto(sockfd, td, sizeof(info_data), 0, (struct sockaddr*)&control_addr, sizeof(control_addr)) < 0){
                perror("sendto");
            }
            close(sockfd);
            exit(EXIT_SUCCESS);
        }

        if(sendto(sockfd, td, sizeof(info_data), 0, (struct sockaddr*)&control_addr, sizeof(control_addr)) < 0){
            perror("sendto");
        }
    }
}

int main(int argc, char* argv[]){
    if(argc != 2){
        fprintf(stderr, "Usage: %s <ID>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    info_data td;
    td.id = atoi(argv[1]);

    if((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0){
        perror("socket");
        exit(EXIT_FAILURE);
    }

    memset(&control_addr, 0, sizeof(control_addr));
    control_addr.sin_family = AF_INET;
    control_addr.sin_port = htons(PORT);
    
    if(inet_pton(AF_INET, "127.0.0.1", &control_addr.sin_addr) <= 0){
        fprintf(stderr, "Indirizzo IP non valido\n");
        exit(EXIT_FAILURE);
    }

    printf("Connesso alla porta %d\n", PORT);

    char buffer[BUFFER_SIZE];
    printf("Inserisci nome: ");
    fgets(buffer, BUFFER_SIZE, stdin);
    buffer[strcspn(buffer, "\n")] = '\0';
    strcpy(td.interprete, buffer);

    if(sendto(sockfd, &td, sizeof(info_data), 0, (struct sockaddr*)&control_addr, sizeof(control_addr)) < 0){
        perror("sendto");
    }

    pthread_t send, recv;
    if(pthread_create(&recv, NULL, (void*)receive_handle, NULL) != 0){
        perror("pthread_create");
        exit(EXIT_FAILURE);
    }

    if(pthread_create(&send, NULL, (void*)handle_info, &td) != 0){
        perror("pthread_create");
        exit(EXIT_FAILURE);
    }

    pthread_join(send, NULL);
    pthread_join(recv, NULL);

    close(sockfd);
}