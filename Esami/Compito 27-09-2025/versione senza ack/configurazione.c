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
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>

#define BUFFER_SIZE 1024
#define PORT 6666        // porta interna
#define EXT_PORT 8080    // porta esterna
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

typedef struct{
    int sockfd_int, sockfd_ext;
    struct sockaddr_in internal_addr;
    socklen_t len;
}interprete_info;


void from_external(void* arg){
    interprete_info* td = (interprete_info*)arg;
    struct sockaddr_in recv_addr;
    socklen_t len = sizeof(recv_addr);

    while(1){
        info_data data;
        ssize_t n = recvfrom(td->sockfd_ext, &data, sizeof(info_data), 0, (struct sockaddr*)&recv_addr, &len);
        if(n == sizeof(data)){
            char buffer[BUFFER_SIZE];
            char ip[INET_ADDRSTRLEN];
            inet_ntop(AF_INET, &recv_addr.sin_addr, ip, INET_ADDRSTRLEN);
            int port = ntohs(recv_addr.sin_port);
            printf("Ricevuto messaggio da Utente [%d, %s, %d]\n", data.id, ip, port); 

            snprintf(buffer, sizeof(buffer), "[ID:%d (%s:%d)]: %s\n", data.id, ip, port, data.msg);
            if(sendto(td->sockfd_int, buffer, strlen(buffer), 0, (struct sockaddr*)&td->internal_addr, td->len) < 0){
                perror("sendto");
            }
        }
    }
}

void from_interprete(void* arg){
    interprete_info* td = (interprete_info*)arg;
    struct sockaddr_in from_addr;
    socklen_t len = sizeof(from_addr);

    while(1){
        info_data data;
        ssize_t n = recvfrom(td->sockfd_int, &data, sizeof(info_data), 0, (struct sockaddr*)&from_addr, &len);
        if(n == sizeof(data)){
            if(!strcmp(data.operation, "messaggio")){
                printf("Invio messaggio '%s' a utente con ID: %d\n", data.msg, data.id_dest);
                
                char buffer[BUFFER_SIZE];
                struct sockaddr_in external_addr;
                memset(&external_addr, 0, sizeof(external_addr));
                external_addr.sin_family = AF_INET;
                external_addr.sin_port = htons(data.port_dest);
                
                inet_pton(AF_INET, data.ip, &external_addr.sin_addr);

                if(sendto(td->sockfd_ext, &data, sizeof(info_data), 0, (struct sockaddr*)&external_addr, sizeof(external_addr)) < 0){
                    perror("sendto");
                }

                snprintf(buffer, sizeof(buffer), "MSG_OK");
                if(sendto(td->sockfd_int, buffer, strlen(buffer), 0, (struct sockaddr*)&from_addr, sizeof(from_addr)) < 0){
                    perror("sendto");
                }
            }
            else if(!strcmp(data.operation, "exit")){
                printf("Chiusura connessione\n");
                close(td->sockfd_ext);
                close(td->sockfd_int);
                exit(EXIT_SUCCESS);
            }
        }
    }
}


int main(){
    int sockfd_int;
    struct sockaddr_in interal_addr, from_addr;
    socklen_t len = sizeof(from_addr);

    int sockfd_ext;
    struct sockaddr_in external_addr;
    if((sockfd_int = socket(AF_INET, SOCK_DGRAM, 0)) < 0){
        perror("socket");
        exit(EXIT_FAILURE);
    }

    memset(&interal_addr, 0, sizeof(interal_addr));
    interal_addr.sin_family = AF_INET;
    interal_addr.sin_port = htons(PORT);
    
    if(inet_pton(AF_INET, "127.0.0.1", &interal_addr.sin_addr) <= 0){
        perror("inet_pton");
        exit(EXIT_FAILURE);
    }

    if(bind(sockfd_int, (struct sockaddr*)&interal_addr, sizeof(interal_addr)) < 0){
        perror("bind");
        exit(EXIT_FAILURE);
    }

    if((sockfd_ext = socket(AF_INET, SOCK_DGRAM, 0)) < 0){
        perror("socket");
        exit(EXIT_FAILURE);
    }

    memset(&external_addr, 0, sizeof(external_addr));
    external_addr.sin_family = AF_INET;
    external_addr.sin_port = htons(EXT_PORT);
    external_addr.sin_addr.s_addr = INADDR_ANY;

    if(bind(sockfd_ext, (struct sockaddr*)&external_addr, sizeof(external_addr)) < 0){
        perror("bind");
        exit(EXIT_FAILURE);
    }
    
    //ricevo il messaggio di riconoscimento di I
    printf("[C] in ascolto porta interna: %d, porta esterna: %d\n", PORT, EXT_PORT);
    info_data data;
    ssize_t n = recvfrom(sockfd_int, &data, sizeof(info_data), 0, (struct sockaddr*)&from_addr, &len);
    if(n == sizeof(data)){
        printf("[C] interprete della macchina: %s\n", data.interprete);
    }

    interprete_info td;
    td.sockfd_int = sockfd_int;
    td.sockfd_ext = sockfd_ext;
    td.internal_addr = from_addr;
    td.len = len;


    pthread_t internal_thread, external_thread;

    if(pthread_create(&internal_thread, NULL, (void*)from_interprete, &td) != 0){
        perror("pthread_create");
        exit(EXIT_FAILURE);
    }

    if(pthread_create(&external_thread, NULL, (void*)from_external, &td) != 0){
        perror("pthread_create");
        exit(EXIT_FAILURE);
    }

    pthread_join(internal_thread, NULL);
    pthread_join(external_thread, NULL);

    close(sockfd_int);
    close(sockfd_ext);
}