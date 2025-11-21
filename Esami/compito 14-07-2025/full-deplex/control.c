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
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <pthread.h>

#define BUFFER_SIZE 1024
#define RECV_PORT 8081
#define PORT 8082
#define MAX_SENSOR 10

typedef struct{
    int sockfd;
    struct sockaddr_in addr;
    int id;
    int value;
}sensor_data;


int log_sockfd;

void handle_allarm(void* arg){
    sensor_data* td = (sensor_data*)arg;
    char buffer[BUFFER_SIZE];

    if(td->value >= 80){
        sleep(10);
        snprintf(buffer, sizeof(buffer), "STOP_ALARM %d", td->id);
    }else{
        sleep(5);
        snprintf(buffer, sizeof(buffer), "STOP_ALARM %d", td->id);
    }

    printf("[Nodo di Controllo] inviato comando di interruzione: %s\n", buffer);
    if(send(td->sockfd, buffer, strlen(buffer), 0) < 0){
        perror("send");
        exit(EXIT_FAILURE);
    }
    free(td);
}

void send_log(void* arg){
    sensor_data* td = (sensor_data*)arg;
    char buffer[BUFFER_SIZE];

    snprintf(buffer, sizeof(buffer), "Sensore ID: %d, value: %d", td->id, td->value);
    printf("[Nodo di Controllo] invio dati al nodo log\n");

    if(send(log_sockfd, buffer, strlen(buffer), 0) < 0){
        perror("send");
        exit(EXIT_FAILURE);
    }
    free(td);
}

void receive_handle(void* arg){
    sensor_data* td = (sensor_data*)arg;
    int sensorfd = td->sockfd;
    char buffer[BUFFER_SIZE];
    char allarm[BUFFER_SIZE];

    while(1){
        ssize_t n = recv(sensorfd, buffer, BUFFER_SIZE - 1,  0);
        if(n <= 0){
            if(n == 0){
                printf("Connessione Chiusa\n");
            }
            perror("recv");
            break;
        }

        buffer[n] = '\0';
        printf("[NODO CONTROLLO]: Ricevuto: %s\n", buffer);

        int id = 0, value = 0;
        if(sscanf(buffer, "Sensore ID: %d, value: %d", &id, &value) == 2){
            sensor_data* td = malloc(sizeof(sensor_data));
            td->id = id;
            td->value = value;
            td->sockfd = sensorfd;
            
            sensor_data* td2 = malloc(sizeof(sensor_data));
            memcpy(td2, td, sizeof(sensor_data));

            pthread_t send_allarm, log;
            if(pthread_create(&send_allarm, NULL, (void*)handle_allarm, td) != 0){
                perror("pthread_create");
                exit(EXIT_FAILURE);
            }

            if(pthread_create(&log, NULL, (void*)send_log, td2) != 0){
                perror("pthread_create");
                exit(EXIT_FAILURE);
            }
            pthread_detach(send_allarm);
            pthread_detach(log);
        }
    }
    close(sensorfd);
    free(td);
}


int main(int argc, char* argv[]){
    if(argc != 2){
        fprintf(stderr, "Usage: %s <ip_log>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    int sockfd;
    struct sockaddr_in central_addr, log_addr;
    
    if((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0){
        perror("sockfd");
        exit(EXIT_FAILURE);
    }

    memset(&central_addr, 0, sizeof(central_addr));
    central_addr.sin_family = AF_INET;
    central_addr.sin_port = htons(RECV_PORT);
    central_addr.sin_addr.s_addr = htonl(INADDR_ANY);

    if(bind(sockfd, (struct sockaddr*)&central_addr, sizeof(central_addr)) < 0){
        perror("bind");
        exit(EXIT_FAILURE);
    }

    if(listen(sockfd, MAX_SENSOR) < 0){
        perror("listen");
        exit(EXIT_FAILURE);
    }

    printf("[Nodo Controllo] in ascolto su porta %d\n", RECV_PORT);

    if((log_sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0){
        perror("sockfd");
        exit(EXIT_FAILURE);
    }

    memset(&log_addr, 0, sizeof(log_addr));
    log_addr.sin_family = AF_INET;
    log_addr.sin_port = htons(PORT);

    if(inet_pton(AF_INET, argv[1], &log_addr.sin_addr) <= 0){
        perror("inet_pton");
        exit(EXIT_FAILURE);
    }

    if(connect(log_sockfd, (struct sockaddr*)&log_addr, sizeof(log_addr)) < 0){
        perror("connect");
        exit(EXIT_FAILURE);
    }

    printf("[Nodo Controllo] connesso su %s:%d\n", argv[1], PORT);
    
    char buffer[BUFFER_SIZE];
    while(1){
        sensor_data* sensor = malloc(sizeof(sensor_data));
        socklen_t len = sizeof(sensor->addr);
        
        if((sensor->sockfd = accept(sockfd, (struct sockaddr*)&sensor->addr, &len)) < 0){
            perror("accept");
            exit(EXIT_FAILURE);
        }
       
        pthread_t recv_data;
        if(pthread_create(&recv_data, NULL, (void*)receive_handle, sensor) != 0){
            perror("pthread_create");
            exit(EXIT_FAILURE);
        }
        pthread_detach(recv_data);
    }

    close(log_sockfd);
    close(sockfd);
}
