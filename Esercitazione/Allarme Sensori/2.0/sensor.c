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
#define PORT 6060

typedef struct{
    int temperatura;
    int umidity;
    int quality_of_air;
    int status;
    int id;
}sensor_data;

int sockfd;

void receive_allarm(void* arg){
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
            printf("[SENSORE-%d] allarme disattivo, procedo a generare nuovi dati\n", td->id);
            td->status = 0;
        }
    }
}

void send_sensordata(void* arg){
    sensor_data* td = (sensor_data*)arg;

    while(1){
        sleep(3);

        if(td->status == 1){
            printf("[SENSORE-%d] in stato di allarme invio sospeso\n", td->id);
            continue;
        }

        td->temperatura = rand()%50;
        td->umidity = rand()%100;
        td->quality_of_air = rand()%100;

        if(td->temperatura >= 30 || td->quality_of_air >= 65){
            printf("[SENSORE-%d] entro in stato di allarme, temperautra: %d, umidità: %d, qualità dell'aria: %d\n", td->id, td->temperatura, td->umidity, td->quality_of_air);
            td->status = 1;
            if(send(sockfd, td, sizeof(sensor_data), 0) < 0){
                perror("send");
                exit(EXIT_FAILURE);
            }
        }else{
            printf("[SENSORE-%d] invio dati al Nodo Centrale, temperatura: %d, umidità: %d, qualità dell'aria: %d\n", td->id, td->temperatura, td->umidity, td->quality_of_air);
            if(send(sockfd, td, sizeof(sensor_data), 0) < 0){
                perror("send");
                exit(EXIT_FAILURE);
            }
        }
    }
}

int main(int argc, char* argv[]){
    if(argc != 3){
        fprintf(stderr, "Usage: %s <ip_centrale> <id>\n", argv[0]);
        exit(EXIT_FAILURE);
    }


    srand(time(NULL));
    struct sockaddr_in central_addr;
    sensor_data td; 
    td.id = atoi(argv[2]);
    td.status = 0;

    if((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0){
        perror("SOCKET");
        exit(EXIT_FAILURE);
    }

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

    printf("[SENSORE-%d] connesso a su %s:%d\n", td.id, argv[1], PORT);

    pthread_t send_thread, recv_thread;

     if(pthread_create(&recv_thread, NULL, (void*)receive_allarm, &td) != 0){
        perror("pthread_create");
        exit(EXIT_FAILURE);
     }

    if(pthread_create(&send_thread, NULL, (void*)send_sensordata, &td) != 0){
        perror("pthread_create");
        exit(EXIT_FAILURE);
    }

    pthread_join(send_thread, NULL);
    pthread_join(recv_thread, NULL);

    close(sockfd);
}