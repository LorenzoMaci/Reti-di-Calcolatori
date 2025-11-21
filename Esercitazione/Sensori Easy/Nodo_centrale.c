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


#define BUFFER_SIZE 1024
#define MAX_SENSORS 100
#define PORT 9000

typedef struct {
    int id; // ID univoco del sensore
    float temperatura; // Temperatura in gradi Celsius
    float umidita; // Umidità in percentuale
    float qualita_aria; // Qualità dell'aria valore da 0 a 100
} SensorData;  

SensorData sensori[MAX_SENSORS]; // Array per memorizzare i dati dei sensori
int sensor_count = 0; // Contatore dei sensori
pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
char ip[INET_ADDRSTRLEN]; // Indirizzo IP del nodo centrale
int port;

void allarme(SensorData* td){
    int sockfd;
    struct sockaddr_in control_addr;
    char buffer[BUFFER_SIZE];
    
    if((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0){
        perror("socket");
        exit(EXIT_FAILURE);
    }

    memset(&control_addr, 0, sizeof(control_addr));
    control_addr.sin_family = AF_INET;
    control_addr.sin_port = htons(port);

    if(inet_pton(AF_INET,  ip , &control_addr.sin_addr) < 0){
        perror("inet_pton");
        exit(EXIT_FAILURE);
    }

    if(connect(sockfd, (struct sockaddr*)&control_addr, sizeof(control_addr)) < 0){
        perror("Connection not established");
        exit(EXIT_FAILURE);
    }

    snprintf(buffer, sizeof(buffer), "Allarme dal sensore ID %d: Temperatura: %f, Umidità: %f, Qualità dell'aria: %f", td->id, td->temperatura, td->umidita, td->qualita_aria);
    
    if(send(sockfd, buffer, strlen(buffer), 0) < 0){
        perror("send");
        exit(EXIT_FAILURE);
    }else{
        printf("Allarme inviato al Nodo di Controllo\n");
    }
    close(sockfd);
}


void handle_node(void* arg){
    SensorData* td = (SensorData*)arg;
    
    if(pthread_mutex_lock(&lock) != 0){
        perror("Pthread_mutex_lock");
        exit(EXIT_FAILURE);
    }

    sensori[sensor_count] = *td;
    sensor_count++;

    if(pthread_mutex_unlock(&lock) != 0){
        perror("Pthread_mutex_unlock");
        exit(EXIT_FAILURE);
    }

    printf("Ricevuto dati dal sensore ID %d: Temperatura: %f, Umidità: %f, Qualità dell'aria: %f\n", td->id, td->temperatura, td->umidita, td->qualita_aria);

    if(td->temperatura > 30.0 || td->qualita_aria < 50.0){
        allarme(td);
    }
}


int main(int argc, char* argv[]){
    if(argc != 3){
        fprintf(stderr, "Usage: %s <IP Nodo Controllo> <Porta Nodo Controllo>\n", argv[0]);
        exit(EXIT_FAILURE);
    }  

    strncpy(ip, argv[1], INET_ADDRSTRLEN);
    port = atoi(argv[2]);
    
    int sockfd;
    struct sockaddr_in server_addr, client_addr;
    socklen_t addr_len = sizeof(client_addr);

    if((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0){
        perror("socket");
        exit(EXIT_FAILURE);
    }

    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(PORT);
    server_addr.sin_addr.s_addr = INADDR_ANY; // Ascolta su tutte le interfacce

    if(bind(sockfd, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0){
        perror("bind");
        exit(EXIT_FAILURE);
    }

    printf("Nodo Centrale in ascolto su %d\n", PORT);
    while (1){
        SensorData* td = malloc(sizeof(SensorData));
        ssize_t n = recvfrom(sockfd, td, sizeof(SensorData), 0, (struct sockaddr*)&server_addr, &addr_len);
        if(n < 0){
            perror("recvfrom");
            continue;
        }

        pthread_t central_node;
        if(pthread_create(&central_node, NULL, (void*)handle_node, td) != 0){
            perror("pthread_create");
            exit(EXIT_FAILURE);
        }

        if(pthread_join(central_node, NULL) != 0){
            perror("pthread_join");
            exit(EXIT_FAILURE);
        }
        free(td);
    }
    close(sockfd);
}