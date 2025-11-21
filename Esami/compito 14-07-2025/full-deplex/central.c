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
#define RECV_PORT 8080
#define PORT 8081
#define MAX_SENSOR 10

typedef struct{
    int id;
    int value;
    int status; //0 non in allarme, 1 in allarme
}sensor_data;

typedef struct{
    int sockfd;
    struct sockaddr_in addr;
    int value;
    int id;
    int status;
}sensor_info;

typedef struct{
    sensor_info* sensors[MAX_SENSOR];
    int counter;
    pthread_mutex_t lock;
}sensor_list;

sensor_list sensors = {.counter = 0, .lock = PTHREAD_MUTEX_INITIALIZER};
int sockfd_control;

void handle_control(void* arg){
    sensor_info* td = (sensor_info*)arg;
    int sockfd = td->sockfd;
    char buffer[BUFFER_SIZE];
    char sensor_ip[INET_ADDRSTRLEN];

    inet_ntop(AF_INET, &td->addr.sin_addr, sensor_ip, INET_ADDRSTRLEN);
    int sensor_port = ntohs(td->addr.sin_port);

    printf("Nuova connessio da %s:%d\n", sensor_ip, sensor_port);

    while(1){
        sensor_data data;
        ssize_t n = recv(sockfd, &data, sizeof(sensor_data), 0);
        if(n <= 0){
            if(n == 0){
                printf("Chiudo connessione\n");
            }
            perror("recv");
            break;
        }   

        pthread_mutex_lock(&sensors.lock);
        int found = 0;
        for(int i = 0; i < sensors.counter; i++){
            if(sensors.sensors[i]->id == data.id){
                found = 1;
                break;
            }
        }

        if(!found && sensors.counter < MAX_SENSOR){
            td->id = data.id;
            td->status = 0;
            td->value = data.value;
            sensors.sensors[sensors.counter++] = td;
        }

        pthread_mutex_unlock(&sensors.lock);

        if(data.value >= 75){
            printf("[NODO CENTRALE]: invio allarme al Nodo di Controllo per value: %d\n", data.value);
            snprintf(buffer, sizeof(buffer), "Sensore ID: %d, value: %d", td->id, td->value);
            if(send(sockfd_control, buffer, strlen(buffer), 0) < 0){
                perror("send");
                exit(EXIT_FAILURE);
            }
        }else{
            printf("[NODO CENTRALE]: ricevuti dati dal Sensore %d: value: %d\n", data.id, data.value);
        }
    }
    close(sockfd);
}

void send_stopallarm(sensor_info* td){
    char buffer[BUFFER_SIZE];
    snprintf(buffer, sizeof(buffer), "STOP_ALARM");

    if(send(td->sockfd, buffer, strlen(buffer), 0) < 0){
        perror("send");
        exit(EXIT_FAILURE);
    }
    printf("[Nodo Centrale] Comando di arresto allarme inviato al sensore ID: %d\n", td->id);
}

void receive_handle(void* arg){
    char buffer[BUFFER_SIZE];
    while(1){
        ssize_t n = recv(sockfd_control, buffer, BUFFER_SIZE -1, 0);
        if(n <= 0){
            if(n == 0){
                printf("connessione chiusa\n");
            }
            perror("recv");
            break;
        }

        buffer[n] = '\0';

        int id = -1;
        if(sscanf(buffer, "STOP_ALARM %d", &id) == 1){
            sensor_info* found = NULL;
            pthread_mutex_lock(&sensors.lock);
            for(int i = 0; i < sensors.counter; i++){
                if(sensors.sensors[i]->id == id){
                    found = sensors.sensors[i];
                    break;
                }
            }
            pthread_mutex_unlock(&sensors.lock);

            if(found){
                send_stopallarm(found);
            }else{
                printf("[NODO CENTRALE] Nodo Sensore %d non trovato\n", id);
            }
        }
    }
}

int main(int argc, char* argv[]){
    if(argc != 2){
        fprintf(stderr, "Usage: %s  <IP_Control>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    int sockfd;
    struct sockaddr_in sensor_addr, control_addr;

    if((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0){
        perror("sockfd");
        exit(EXIT_FAILURE);
    }

    memset(&sensor_addr, 0, sizeof(sensor_addr));
    sensor_addr.sin_family = AF_INET;
    sensor_addr.sin_port = htons(RECV_PORT);
    sensor_addr.sin_addr.s_addr = htonl(INADDR_ANY);

    if(bind(sockfd, (struct sockaddr*)&sensor_addr, sizeof(sensor_addr)) < 0){
        perror("bind");
        exit(EXIT_FAILURE);
    }

    if(listen(sockfd, MAX_SENSOR) < 0){
        perror("listen");
        exit(EXIT_FAILURE);
    }

    printf("[NODO CENTRALE] in ascolto su %d\n", RECV_PORT);

    if((sockfd_control = socket(AF_INET, SOCK_STREAM, 0)) < 0){
        perror("socket");
        exit(EXIT_FAILURE);
    }

    memset(&control_addr, 0, sizeof(control_addr));
    control_addr.sin_family = AF_INET;
    control_addr.sin_port = htons(PORT);
    
    if(inet_pton(AF_INET, argv[1], &control_addr.sin_addr) <= 0){
        perror("inet_pton");
        exit(EXIT_FAILURE);
    }

    if(connect(sockfd_control, (struct sockaddr*)&control_addr, sizeof(control_addr)) < 0){
        perror("connect");
        exit(EXIT_FAILURE);
    }

    printf("[NODO CENTRALE] connesso su %s:%d\n", argv[1], PORT);

    pthread_t recv_thread;

    if(pthread_create(&recv_thread, NULL, (void*)receive_handle, NULL) != 0){
        perror("pthread_create");
        exit(EXIT_FAILURE);
    }

    pthread_detach(recv_thread);

    while(1){
        sensor_info* sensor = malloc(sizeof(sensor_info));
        socklen_t len = sizeof(sensor->addr);

        if((sensor->sockfd = accept(sockfd, (struct sockaddr*)&sensor->addr, &len)) < 0){
            perror("accept");
            free(sensor);
            continue;
        }

        pthread_t sensor_thread;
        if(pthread_create(&sensor_thread, NULL, (void*)handle_control, sensor)  != 0){
            perror("pthread_create");
            close(sensor->sockfd);
            free(sensor);
        }

        pthread_detach(sensor_thread);    
    }
    close(sockfd_control);
    close(sockfd);
    pthread_mutex_destroy(&sensors.lock);
}

