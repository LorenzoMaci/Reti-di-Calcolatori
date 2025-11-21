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
#define PORT 8080
#define SEND_PORT 8081
#define BUFFER_SIZE 1024

typedef struct{
    int temperatura;
    int umidity;
    int quality_air;
    int id;
}sensor_data;

typedef struct{
    int sockfd;
    struct sockaddr_in addr;
    int temperatura;
    int quality_air;
    int umidity;
    int id;
}sensor_info;

typedef struct{
    sensor_info* sensors[MAX_SENSORS];
    int counter;
    pthread_mutex_t lock;
}sensor_list;

sensor_list sensors = {.counter = 0, .lock = PTHREAD_MUTEX_INITIALIZER};
char ip_control[INET_ADDRSTRLEN];

void send_allarm(sensor_data* td){
    int sockfd;
    char buffer[BUFFER_SIZE];
    struct sockaddr_in control_addr;

    if((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0){
        perror("socket");
        exit(EXIT_FAILURE);
    }

    memset(&control_addr, 0, sizeof(control_addr));
    control_addr.sin_family = AF_INET;
    control_addr.sin_port = htons(SEND_PORT);
    
    if(inet_pton(AF_INET, ip_control, &control_addr.sin_addr) <= 0){
        perror("inet_pton");
        exit(EXIT_FAILURE);
    }

    if(connect(sockfd, (struct sockaddr*)&control_addr, sizeof(control_addr)) < 0){
        perror("connect");
        exit(EXIT_FAILURE);
    }

    snprintf(buffer, sizeof(buffer), "Allarme dal Sensore: %d, Temperatura: %d, umidità: %d, qualità dell'aria: %d\n", td->id, td->temperatura, td->umidity, td->quality_air);

    if(send(sockfd, buffer, strlen(buffer), 0) < 0){
        perror("send");
        exit(EXIT_FAILURE);
    }
    close(sockfd);
}

int main(int argc, char* argv[]){
    if(argc != 2){
        fprintf(stderr, "Usage: %s <ip_controllo>\n", argv[0]);
        exit(EXIT_FAILURE);
    }


    int sockfd;
    struct sockaddr_in recv_addr, sensor_addr;
    socklen_t len = sizeof(sensor_addr);
    strncpy(ip_control, argv[1], INET_ADDRSTRLEN);

    if((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0){
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

    printf("[Nodo centrale] in ascolto su porta %d\n", PORT);

    while(1){
        sensor_data td;
        ssize_t n = recvfrom(sockfd, &td, sizeof(sensor_data), 0, (struct sockaddr*)&sensor_addr, &len);
        if(n < 0){
            perror("recvfrom");
        }

        pthread_mutex_lock(&sensors.lock);
        int found = 0;
        for(int i = 0;  i < sensors.counter; i++){
            if(sensors.sensors[i]->id == td.id){ // vedo solo se c'è l'ID 
                found = 1;
                break;
            }
        }

        if(!found && sensors.counter < MAX_SENSORS){
            sensor_info* new_sensor = malloc(sizeof(sensor_info));
            new_sensor->id = td.id;
            new_sensor->quality_air = td.quality_air;
            new_sensor->temperatura = td.temperatura;
            new_sensor->umidity = td.umidity;
            sensors.sensors[sensors.counter++] = new_sensor;
        }

        pthread_mutex_unlock(&sensors.lock);

        printf("[NODO CENTRALE]: dati dal nodo Sensore-%d: Temperatura: %d, umidità: %d, qualità dell'aria: %d\n", td.id, td.temperatura, td.umidity, td.quality_air);

        if(td.temperatura > 30 || td.quality_air >= 66){
            printf("[NODO CENTRALE]: Invio allarme al nodo di Controllo\n");
            send_allarm(&td);
        }
    }
    close(sockfd);
}