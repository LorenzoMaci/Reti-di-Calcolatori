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
Memorizzerà un file di LOG di tutti gli allarmi*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <pthread.h>
#include <netinet/in.h>

typedef struct{
    int id; // ID univoco del sensore
    float temperatura; // Temperatura in gradi Celsius;
    float umidita; // Umidità in percentuale
    float qualita_aria; // Qualità dell'aria (valore da 0 a 100)
}SensorData;

int sockfd;

void send_data(void* arg){
    SensorData* td = (SensorData*)arg;
    while(1){
        td->temperatura = rand()% 50; 
        td->qualita_aria = rand() % 100 + 1; // Simula un valore di qualità dell'aria
        td->umidita = rand() % 100; // Simula un valore di umidità
        printf("Comunico con il nodo centrale..\n");
        printf("Sensore ID %d: invio dati Temperatura: %f Umidità: %f Qualità dell'aria: %f\n", td->id + 1, td->temperatura, td->umidita, td->qualita_aria);
        if(send(sockfd, td, sizeof(SensorData), 0) < 0){
            perror("send");
            exit(EXIT_FAILURE);
        }
        sleep(3); // Attende 3 secondi prima di inviare nuovamente i dati
    }
}

int main(int argc, char* argv[]){
    if(argc != 4){
        fprintf(stderr, "Usage: %s <Indirizzo IP> <Porta> <ID> \n", argv[0]);
        exit(EXIT_FAILURE);
    }

    struct sockaddr_in addr;
    SensorData sensor_data;
    sensor_data.id = atoi(argv[3]); // ID del sensore

    if((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0){ 
        perror("socket");
        exit(EXIT_FAILURE);
    }

    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(atoi(argv[2]));


    if(inet_pton(AF_INET, argv[1], &addr.sin_addr) <= 0){ //converte il testo indirizzo IPv4 in una struttura di rete
        perror("inet_pton");
        exit(EXIT_FAILURE);
    }

    if(connect(sockfd, (struct sockaddr*)&addr, sizeof(addr)) < 0){
        perror("connect");
        exit(EXIT_FAILURE);
    }

    printf("Connesso a %s:%s\n", argv[1], argv[2]);

    pthread_t send_thread;
    if(pthread_create(&send_thread, NULL, (void*)send_data, &sensor_data) != 0){
        perror("pthread-create");
        exit(EXIT_FAILURE);
    }

    if(pthread_join(send_thread, NULL) != 0){
        perror("Pthread_join");
        exit(EXIT_FAILURE);
    }
    close(sockfd);
}

