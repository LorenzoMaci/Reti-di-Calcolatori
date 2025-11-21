/*
Creare 2 sensori, in C che prendono in input un id, 
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
#define PORT 5050

typedef struct{
    int id;
    int value;
    int status;
    int counter_allarm;
    int counter_stopallarm;
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
            printf("[SENSORE-%d] allarme disattivo, procedo con il generare nuovi dati\n", td->id);
            td->status = 0;
            td->counter_stopallarm++;
            fflush(stdout);
        }
    }
}

void handle_data(void* arg){
    sensor_data* td = (sensor_data*)arg;

    while(1){
        sleep(5);

        if(td->status == 1){
            printf("[SENSORE-%d] in stato di allarme\n", td->id);
            continue;
        }

        td->value = rand()%(97 - 63 + 1) + 63;

        if(td->value >= 75){
            printf("[SENSORE-%d] entro in stato di allarme informo il nodo centrale, value: %d\n", td->id, td->value);
            td->status = 1;
            td->counter_allarm++;
            if(send(sockfd, td, sizeof(sensor_data), 0) < 0){
                perror("send");
                exit(EXIT_FAILURE);
            }
        }else{
            printf("[SENSORE-%d] invio dati al nodo centrale, value: %d\n", td->id, td->value);
            if(send(sockfd, td, sizeof(sensor_data), 0) < 0){
                perror("send");
                exit(EXIT_FAILURE);
            }
        }
    }
}

int main(int argc, char* argv[]){
    if(argc != 3){
        fprintf(stderr, "Usage: %s <ip_central> <ID>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    srand(time(NULL));

    sensor_data td;
    td.id = atoi(argv[2]);
    td.status = 0;
    struct sockaddr_in central_addr;

    if((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0){
        perror("socket");
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

    printf("[SENSORE-%d] connesso a %s:%d\n", td.id, argv[1], PORT);

    pthread_t send_thread, recv_thread;

    if(pthread_create(&recv_thread, NULL, (void*)receive_handle, &td) != 0){
        perror("pthread_create");
        exit(EXIT_FAILURE);
    }

    if(pthread_create(&send_thread, NULL, (void*)handle_data, &td) != 0){
        perror("pthread_create");
        exit(EXIT_FAILURE);
    }

    pthread_join(send_thread, NULL);
    pthread_join(recv_thread, NULL);

    close(sockfd);
}