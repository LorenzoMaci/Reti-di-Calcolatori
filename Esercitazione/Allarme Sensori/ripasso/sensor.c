/*Nodo Sensore:
Può inviare dati ogni 3 secondi
a comunicazione deve essere affidabile
I dati del sensore possono essere temperatura, umidità e qualità dell'aria
Ogni sensore deve essere identificato da un ID univoco
Se la temperatura è superiore a 30°C o la qualità dell'aria è scarsa, deve entrare in modalità Allarme
In modalità Allarme non è possibile inviare dati del sensore
Quando riceve un comando per interrompere la modalità allarme, tornerà in modalità normale
*/


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <time.h>

#define BUFFER_SIZE 1024
#define PORT 7070

typedef struct{
    int temperatura;
    int umidity;
    int quality_of_air;
    int status;
    int id;
}sensor_data;

int sockfd;

void receive_handle(void* arg){
    sensor_data* td = (sensor_data*)arg;
    char buffer[BUFFER_SIZE];

    while(1){
        ssize_t n = recv(sockfd, buffer, BUFFER_SIZE - 1, 0);

        if(n <= 0){
            if(n == 0){
                printf("Chiusura connessione\n");
            }
            perror("recv");
            exit(EXIT_FAILURE);
        }

        buffer[n] = '\0';

        if(!strcmp(buffer, "STOP_ALARM")){
            printf("[SENSORE-%d] allarme disattivato dal Nodo Centrale\n", td->id);
            td->status = 0;
            fflush(stdout);
        }
    }
}

void handle_data(void* arg){
    sensor_data* td = (sensor_data*)arg;

    while(1){
        sleep(3);

        if(td->status == 1){
            printf("[SENSORE-%d] in stato di allarme invio sospeso\n", td->id);
            continue;
        }

        td->temperatura = rand()% 50;
        td->umidity = rand()%100;
        td->quality_of_air = rand()%100;

        if(td->temperatura >= 30 || td->quality_of_air >= 66){
            printf("[SENSORE-%d] entro in stato di allarme, invio dati: temperatura: %d, umidità: %d, qualità dell'aria: %d\n", td->id, td->temperatura, td->umidity , td->quality_of_air);
            td->status = 1;
            if(send(sockfd, td, sizeof(sensor_data), 0) < 0){
                perror("send");
                exit(EXIT_FAILURE);
            }
        }else{
            printf("[SENSORE-%d] invio dati: temperatura: %d, umidità: %d, qualità dell'aria: %d\n", td->id, td->temperatura, td->umidity , td->quality_of_air);
            if(send(sockfd, td, sizeof(sensor_data), 0) < 0){
                perror("send");
                exit(EXIT_FAILURE);
            }
        }
    }
}

int main(int argc, char* argv[]){
    if(argc != 3){
        fprintf(stderr, "Usage: %s <IP_Central> <ID>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    srand(time(NULL));

    if((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0){
        perror("socket");
        exit(EXIT_FAILURE);
    }

    sensor_data data;
    data.id = atoi(argv[2]);
    data.status = 0;
    struct sockaddr_in central_addr;

    memset(&central_addr, 0, sizeof(central_addr));
    central_addr.sin_family = AF_INET;
    central_addr.sin_port = htons(PORT);

    if(inet_pton(AF_INET, argv[1], &central_addr.sin_addr) <= 0){
        perror("inet_pton");
        exit(EXIT_FAILURE);
    }

    if(connect(sockfd, (struct sockaddr*)&central_addr, sizeof(central_addr)) < 0){
        perror("connect");
        exit(EXIT_FAILURE);
    }

    printf("[SENSORE-%d] connesso a %s:%d\n", data.id, argv[1], PORT);

    pthread_t recv_thread, send_thread;

    if(pthread_create(&recv_thread, NULL, (void*)receive_handle, &data) != 0){
        perror("pthread_create");
        exit(EXIT_FAILURE);
    }

    if(pthread_create(&send_thread, NULL, (void*)handle_data, &data) != 0){
        perror("pthread_create");
        exit(EXIT_FAILURE);
    }

    pthread_join(send_thread, NULL);
    pthread_join(recv_thread, NULL);

    close(sockfd);
}