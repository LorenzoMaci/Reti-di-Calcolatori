/*Sviluppare una soluzione software che consenta a un Nodo Sensore di inviare alcuni dati dei sensori a uno specifico Nodo Centrale.
Quando un Nodo Centrale riceve dati dal sensore, se si verifica una condizione specifica, il Nodo Centrale invierà un allarme a un Nodo di Controllo.

Nodo Sensore:
Può inviare dati ogni 3 secondi
La comunicazione potrebbe non essere affidabile
I dati del sensore potrebbero essere temperatura, umidità e qualità dell'aria
Ogni sensore deve essere identificato da un ID univoco

Nodo Centrale:
Memorizzerà un nuovo sensore quando riceve un messaggio da esso
Se la temperatura è superiore a 30°C o la qualità dell'aria è scarsa, deve inviare un messaggio al Nodo di Controllo
La comunicazione deve essere affidabile

Nodo di Controllo:
Stamperà gli allarmi
Memorizzerà un file di LOG di tutti gli allarmi
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <time.h>

#define MAX_SENSORS 10
#define PORT 8081
#define BUFFER_SIZE 1024

int main(){
    int sockfd;
    char buffer[BUFFER_SIZE];
    struct sockaddr_in recv_addr;

    if((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0){
        perror("socket");
        exit(EXIT_FAILURE);
    }

    memset(&recv_addr, 0, sizeof(recv_addr));
    recv_addr.sin_family = AF_INET;
    recv_addr.sin_port = htons(PORT);
    recv_addr.sin_addr.s_addr = htonl(INADDR_ANY);

    if(bind(sockfd, (struct sockaddr*)&recv_addr, sizeof(recv_addr)) < 0){
        perror("bind");
        exit(EXIT_FAILURE);
    }

    if(listen(sockfd, MAX_SENSORS) < 0){
        perror("listen");
        exit(EXIT_FAILURE);
    }

    FILE* f;
    if((f = fopen("file.txt", "a+")) == NULL){
        perror("fopen");
        exit(EXIT_FAILURE);
    }

    while(1){
        int sensorfd;
        struct sockaddr_in sensor_addr;
        socklen_t len = sizeof(sensor_addr);

        if((sensorfd = accept(sockfd, (struct sockaddr*)&sensor_addr, &len)) < 0){
            perror("accept");
            exit(EXIT_FAILURE);
        }
        ssize_t n = recv(sensorfd, buffer, BUFFER_SIZE -1, 0);
        if(n > 0){
            buffer[n] = '\0';
            fprintf(f, "%s\n", buffer);
            fflush(f);
            printf("[NODO CONTROLLO] RIcevuto: %s\n", buffer);
        }else if(n <= 0){
            if(n == 0){
                printf("connessione chiusa dal Nodo di controllo\n");
            }
            perror("recv");
            exit(EXIT_FAILURE);
        }
        close(sensorfd);
    }
    fclose(f);
    close(sockfd);
}

