/*
Creare 2 sensori,  in c che prendono in input un id, 
ogni 5 sec generavano un numero casuale  tra 63 e 97 e lo inviavano ad un central node. 
Se il numero era maggiore di 75 entra in uno stato di allarme e smette di inviare messaggi fino a quando non viene sbloccato.
Il central node riceve le misurazioni dei sensori e se maggiori di 75 trasmette id e misurazione ad un control node.
Il control node riceve le misurazioni e se il valore è <80 aspetta 5 sec ed invia un messaggio di sblocco del sensore al central node 
che poi lo girerà al sensore, se >80 aspetterà 10 sec prima di sbloccare il sensore.
Infine un log node che registra in un file tutte le misurazioni di tutti i sensori, gli allarmi e gli sblocchi
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <time.h>

#define BUFFER_SIZE 1024
#define PORT 5070
#define MAX_SENSORS 10

int main(){
    int sockfd;
    struct sockaddr_in log_addr;

    if((sockfd =  socket(AF_INET, SOCK_STREAM, 0)) < 0){
        perror("socket");
        exit(EXIT_FAILURE);
    }

    memset(&log_addr, 0, sizeof(log_addr));
    log_addr.sin_family = AF_INET;
    log_addr.sin_port = htons(PORT);
    log_addr.sin_addr.s_addr = htonl(INADDR_ANY);

    if(bind(sockfd, (struct sockaddr*)&log_addr, sizeof(log_addr)) < 0){
        perror("bind");
        exit(EXIT_FAILURE);
    }

    if(listen(sockfd, MAX_SENSORS) < 0){
        perror("listen");
        exit(EXIT_FAILURE);
    }

    printf("[NODO LOG] in ascolto su %d\n", PORT);

    FILE* f;
    if((f = fopen("file.log", "a")) == NULL){
        perror("fopen");
        exit(EXIT_FAILURE);
    }

    int sockfd_control;
    struct sockaddr_in control_addr;
    socklen_t len = sizeof(control_addr);
    char buffer[BUFFER_SIZE];

    if((sockfd_control = accept(sockfd, (struct sockaddr*)&control_addr, &len)) < 0){
        perror("accept");
        exit(EXIT_FAILURE);
    }

    while(1){
        ssize_t n = recv(sockfd_control,buffer, BUFFER_SIZE - 1, 0);

        if(n <= 0){
            if(n == 0){
                printf("Chiusura connessione\n");
            }
            perror("recv");
            break;
        }

        buffer[n] = '\0';

        printf("[NODO LOG] Ricevuto: %s\n", buffer);
        fprintf(f, "%s\n", buffer);
        fflush(f);
    }
    fclose(f);
    close(sockfd_control);
    close(sockfd);
}