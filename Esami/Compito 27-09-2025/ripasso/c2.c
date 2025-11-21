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
#include <sys/socket.h>
#include <pthread.h>

#define BUFFER_SIZE 1024
#define MAX_SIZE 100
#define PORT 7000
#define EXTERNAL_PORT 7777

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

typedef struct{
    int sockfd_in, sockfd_ext;
    struct sockaddr_in interprete_addr;
    socklen_t len;
}interprete_info;

void from_external(void* arg){
    interprete_info* td = (interprete_info*)arg;
    struct sockaddr_in external_addr;
    socklen_t len = sizeof(external_addr);
    interprete_data data;
    char buffer[BUFFER_SIZE];

    while(1){
        ssize_t n = recvfrom(td->sockfd_ext, &data, sizeof(interprete_data), 0, (struct sockaddr*)&external_addr, &len);
        if(n == sizeof(data)){
            char ip[INET_ADDRSTRLEN];
            int port = ntohs(external_addr.sin_port);

            inet_ntop(AF_INET, &external_addr.sin_addr, ip, INET_ADDRSTRLEN);
            printf("[CONFIGURAZIONE] Ricevuto messaggio dall'utente %d esterno (IP:%s, Port: %d)\n", data.id, ip, port);
            snprintf(buffer, sizeof(buffer), "[%d (%s:%d)]: %s", data.id, ip, port, data.msg);
            if(sendto(td->sockfd_in, buffer, strlen(buffer), 0, (struct sockaddr*)&td->interprete_addr, td->len) < 0){
                perror("sendto");
            }
        }
    }
}

void from_internal(void* arg){
    interprete_info* td = (interprete_info*)arg;
    struct sockaddr_in from_addr;
    socklen_t len = sizeof(from_addr);
    char ack[BUFFER_SIZE];
    interprete_data data;

    while(1){
        ssize_t n = recvfrom(td->sockfd_in, &data, sizeof(interprete_data), 0, (struct sockaddr*)&from_addr, &len);
        if(n == sizeof(data)){
            if(!strcmp(data.operation, "messaggio")){
                printf("[CONFIGURAZIONE] invio messaggio '%s' a (%s:%d)\n", data.msg, data.ip, data.external_port);

                struct sockaddr_in extinterface_addr;
                memset(&extinterface_addr, 0, sizeof(extinterface_addr));
                extinterface_addr.sin_family = AF_INET;
                extinterface_addr.sin_port = htons(data.external_port);
                
                if(inet_pton(AF_INET, data.ip, &extinterface_addr.sin_addr) <= 0){
                    perror("inet_pton");
                    exit(EXIT_FAILURE);
                }

                if(sendto(td->sockfd_ext, &data, sizeof(interprete_data), 0, (struct sockaddr*)&extinterface_addr, sizeof(extinterface_addr)) < 0){
                    perror("sendto");
                }

                snprintf(ack, sizeof(ack), "ACK_%d", data.seq);
                if(sendto(td->sockfd_in, ack, strlen(ack), 0, (struct sockaddr*)&from_addr, sizeof(from_addr)) < 0){
                    perror("sendto");
                }
            }
            else if(!strcmp(data.operation, "exit")){
                printf("Chiusura connessione\n");
                close(td->sockfd_ext);
                close(td->sockfd_in);
                exit(EXIT_SUCCESS);
            }
        }
    }
}

int main(){
    int sockfd_in;
    struct sockaddr_in configuration_addr, from_addr;
    socklen_t len = sizeof(from_addr);

    if((sockfd_in = socket(AF_INET, SOCK_DGRAM, 0)) < 0){
        perror("socket");
        exit(EXIT_FAILURE);
    }

    memset(&configuration_addr, 0, sizeof(configuration_addr));
    configuration_addr.sin_family = AF_INET;
    configuration_addr.sin_port = htons(PORT);

    if(inet_pton(AF_INET, "127.0.0.2", &configuration_addr.sin_addr) <= 0){
        perror("inet_pton");
        exit(EXIT_FAILURE);
    }

    if(bind(sockfd_in, (struct sockaddr*)&configuration_addr, sizeof(configuration_addr)) < 0){
        perror("bind");
        exit(EXIT_FAILURE);
    }

    int sockfd_ext;
    struct sockaddr_in external_addr;

    if((sockfd_ext = socket(AF_INET, SOCK_DGRAM, 0)) < 0){
        perror("socket");
        exit(EXIT_FAILURE);
    }

    memset(&external_addr, 0, sizeof(external_addr));
    external_addr.sin_family = AF_INET;
    external_addr.sin_port = htons(EXTERNAL_PORT);
    external_addr.sin_addr.s_addr = htonl(INADDR_ANY);

    if(bind(sockfd_ext, (struct sockaddr*)&external_addr, sizeof(external_addr)) < 0){
        perror("bind");
        exit(EXIT_FAILURE);
    }

    printf("[CONFIGURAZIONE] In ascolto su porta interna %d e porta esterna %d\n", PORT, EXTERNAL_PORT);
    printf("[CONFIGURAZIONE] Aspetto messaggio di riconoscimento dall'interprete\n");

    interprete_data data;
    ssize_t n = recvfrom(sockfd_in, &data, sizeof(interprete_data), 0, (struct sockaddr*)&from_addr, &len);
    if (n == sizeof(data)) {
        printf("[CONFIGURAZIONE] Macchina di: %s\n", data.interface);
    }
    
    interprete_info td;
    td.sockfd_in = sockfd_in;
    td.sockfd_ext = sockfd_ext;
    td.interprete_addr = from_addr;
    td.len = len;

    pthread_t ext_thread, internal_thread;

    if (pthread_create(&ext_thread, NULL, (void*)from_external, &td) != 0) {
        perror("pthread_Create");
        exit(EXIT_FAILURE);
    }

    if (pthread_create(&internal_thread, NULL, (void*)from_internal, &td) != 0) {
        perror("pthread_create");
        exit(EXIT_FAILURE);
    }

    pthread_join(internal_thread, NULL);
    pthread_join(ext_thread, NULL);

    close(sockfd_ext);
    close(sockfd_in);
}