/*
Creare 2 sensori, uno in c ed uno in pyton che prendevano in input un id, 
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

#define BUFFER_SIZE 1024
#define PORT 9090 //porta di invio per il central node
#define RECV_PORT 9089 //porta di ricezione messaggi

typedef struct{
    int id;
    int value;
    int status;
    int counter_allarm;
    int counter_disableallarm;
}sensor_data;

int sockfd;

void recive_handler(void* arg){
    sensor_data* td = (sensor_data*)arg;
    char buffer[BUFFER_SIZE];

    while(1){
        ssize_t n = recv(sockfd, buffer, BUFFER_SIZE - 1, 0);
        if(n <= 0){
            if(n == 0){
                printf("Connessione chiusa dal server\n");
            }
            perror("recv");
            exit(EXIT_FAILURE);
        }

        buffer[n] = '\0';

        if(!strcmp(buffer, "STOP_ALARM")){
            printf("[SENSOR-%d] allarme disattivato dal nodo centrale, procedo con l'invio di altri dati\n", td->id);
            td->status = 0;
            td->counter_disableallarm++;
        }
        else if(!strcmp(buffer, "exit")){
            printf("Chiusa connessione dal server\n");
            exit(EXIT_SUCCESS);
        }
    }
}

void send_handler(void* arg){
    sensor_data* td = (sensor_data*)arg;

    while(1){
        sleep(5);

        if(td->status == 1){
            printf("[SENSOR-%d] in stato di allarme, invio sospeso\n", td->id);
            continue;
        }

        td->value = rand()%(97 - 63 + 1) + 63;

        if(td->value >= 75){
            printf("[SENSOR-%d] Entro in stato di allarme\n", td->id);
            printf("[SENSOR-%d] Invio allarme al Nodo Centrale con valore: %d\n", td->id, td->value);
            td->counter_allarm++;
            td->status = 1;
            if(send(sockfd, td, sizeof(sensor_data), 0) < 0){
                perror("send");
                exit(EXIT_FAILURE);
            }
        }else{
            printf("[SENSOR-%d] invio dati al Nodo centrale con valore: %d\n", td->id, td->value);
            if(send(sockfd, td, sizeof(sensor_data), 0) < 0){
                perror("send");
                exit(EXIT_FAILURE);
            }
        }
    }
}

int main(int argc, char* argv[]){
    if(argc != 4){
        fprintf(stderr, "Usage: %s <ip_central> <ip_sensore> <ID>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    srand(time(NULL));
    int id = atoi(argv[3]);
    sensor_data sensor;
    sensor.id = id;
    sensor.status = 0;
    struct sockaddr_in central_addr, recv_addr;

    if((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0){
        perror("socket");
        exit(EXIT_FAILURE);
    }

    memset(&recv_addr, 0, sizeof(recv_addr));
    recv_addr.sin_family = AF_INET;
    recv_addr.sin_port = htons(RECV_PORT);
    
    if(inet_pton(AF_INET, argv[2], &recv_addr.sin_addr) <= 0){
        perror("inet_pton");
        exit(EXIT_FAILURE);
    }

    if(bind(sockfd, (struct sockaddr*)&recv_addr, sizeof(recv_addr)) < 0){
        perror("bind");
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

    printf("Connesso presso il nodo centrale %s:%d\n", argv[1], PORT);

    pthread_t recv_thread, send_thread;

    if(pthread_create(&recv_thread, NULL, (void*)recive_handler, &sensor) < 0){
        perror("pthread_create");
        exit(EXIT_FAILURE);
    }

    if(pthread_create(&send_thread, NULL, (void*)send_handler, &sensor) < 0){
        perror("pthread_create");
        exit(EXIT_FAILURE);
    }

    pthread_join(send_thread, NULL);
    pthread_join(recv_thread, NULL);

    close(sockfd);
}