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

#define PORT 8080

typedef struct{
    int temperatura;
    int umidity;
    int quality_air;
    int id;
}sensor_data;


int main(int argc, char* argv[]){
    if(argc != 3){
        fprintf(stderr, "Usage: %s <nodo_centrale ip> <ID>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    int sockfd;
    sensor_data td;
    td.id = atoi(argv[2]);
    struct sockaddr_in central_addr;

    if((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0){
        perror("sockfd");
        exit(EXIT_FAILURE);
    }

    memset(&central_addr, 0, sizeof(central_addr));
    central_addr.sin_family = AF_INET;
    central_addr.sin_port = htons(PORT);

    if(inet_pton(AF_INET, argv[1], &central_addr.sin_addr) <= 0){
        perror("inet_pton");
        exit(EXIT_FAILURE);
    }

    printf("[Sensor-%d] connesso al server: %s:%d", td.id, argv[1], PORT);

    srand(time(NULL));
    while(1){
        sleep(3);

        td.temperatura = rand()%50;
        td.umidity = rand()%90;
        td.quality_air = rand()%100;
    
        if(sendto(sockfd, &td, sizeof(td), 0, (struct sockaddr*)&central_addr, sizeof(central_addr)) < 0){
            perror("sendto");
        }
        printf("[SENSORE-%d] Invio dati al nodo centrale: Temperatura: %d, umidità: %d, qualità dell'aria: %d\n", td.id, td.temperatura, td.umidity,td.quality_air);
    }
    close(sockfd);
}
