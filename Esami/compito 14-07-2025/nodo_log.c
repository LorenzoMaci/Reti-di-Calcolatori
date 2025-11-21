/*
Creare 2 sensori,in c che prendevano in input un id, 
ogni 5 sec generavano un numero casuale  tra 63 e 97 e lo inviavano ad un central node. 
Se il numero era maggiore di 75 entra in uno stato di allarme e smette di inviare messaggi fino a quando non viene sbloccato.
Il central node riceve le misurazioni dei sensori e se maggiori di 75 trasmette id e misurazione ad un control node.
Il control node riceve le misurazioni e se il valore è <80 aspetta 5 sec ed invia un messaggio di sblocco del sensore al central node 
che poi lo girerà al sensore, se >80 aspetterà 10 sec prima di sbloccare il sensore.
Infine un log node che registra in un file tutte le misurazioni di tutti i sensori, gli allarmi e gli sblocchi*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <time.h>
#include <netinet/in.h>
#include <pthread.h>

#define PORT 9093
#define BUFFER_SIZE 1024

int main(){
    int sockfd;
    struct sockaddr_in control_addr;

    if((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0){
        perror("sockfd");
        exit(EXIT_FAILURE);
    }

    memset(&control_addr, 0, sizeof(control_addr));
    control_addr.sin_family = AF_INET;
    control_addr.sin_port = htons(PORT);
    control_addr.sin_addr.s_addr = htonl(INADDR_ANY);

    if(bind(sockfd, (struct sockaddr*)&control_addr, sizeof(control_addr)) < 0){
        perror("bind");
        exit(EXIT_FAILURE);
    }

    if(listen(sockfd, 10) < 0){
        perror("listen");
        exit(EXIT_FAILURE);
    }

    FILE* f;
    char buffer[BUFFER_SIZE];

    if((f = fopen("node.log", "a+")) == NULL){
        perror("fopen");
        exit(EXIT_FAILURE);
    }

    while(1){
        int clientfd;
        struct sockaddr_in sensor_addr;
        socklen_t len = sizeof(sensor_addr);
        
        if((clientfd = accept(sockfd, (struct sockaddr*)&sensor_addr, &len)) < 0){
            perror("accept");
            exit(EXIT_FAILURE);
        }

        ssize_t n = recv(clientfd, buffer, BUFFER_SIZE - 1,  0);
        if(n > 0){
            buffer[n] = '\0';
            fprintf(f, "%s\n", buffer);
            fflush(f);
            printf("[LOG NODE] Ricevuto: %s\n", buffer);
        }else if(n <= 0){
            if(n == 0){
                printf("connessione chiusa dal Nodo di controllo\n");
            }
            perror("recv");
            exit(EXIT_FAILURE);
        }
        close(clientfd);
    }

    fclose(f);
    close(sockfd);
}