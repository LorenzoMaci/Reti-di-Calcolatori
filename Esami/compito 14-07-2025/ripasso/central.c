/*
Creare 2 sensori,  in c che prende in input un id, 
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
#define PORT_CONTROL 5060
#define MAX_SENSORS 10

typedef struct{
    int id;
    int value;
    int status;
    int counter_allarm;
    int counter_stopallarm;
}sensor_data;

typedef struct{
    int sockfd;
    struct sockaddr_in sensor_addr;
    int id;
    int value;
    int status;
}sensor_info;

typedef struct{
    sensor_info* sensors[MAX_SENSORS];
    int counter;
    pthread_mutex_t lock;
}sensor_list;

sensor_list sensors = {.counter = 0, .lock = PTHREAD_MUTEX_INITIALIZER};
int sockfd_control;

void send_stopallarm(sensor_info* td){
    char buffer[BUFFER_SIZE];
    snprintf(buffer, sizeof(buffer), "STOP_ALARM");

    printf("[NODO CENTRALE] invio %s al SENSORE-%d\n", buffer, td->id);

    if(send(td->sockfd, buffer, strlen(buffer), 0) < 0){
        perror("send");
        exit(EXIT_FAILURE);
    }

}

void receive_control(void* arg){
    char buffer[BUFFER_SIZE];

    while(1){
        ssize_t n = recv(sockfd_control, buffer, BUFFER_SIZE - 1, 0);
        if(n <= 0){
            if(n == 0){
                printf("Chiusura connessione\n");
            }
            perror("recv");
            exit(EXIT_FAILURE);
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

            if(found != NULL){
                printf("[NODO CENTRALE] SENSORE-%d trovato\n", found->id);
                send_stopallarm(found);   
            }else{
                printf("[NODO CENTRALE] SENSORE-%d non trovato\n", id);
            }
        }
    }
}

void handle_data(void* arg){
    sensor_info* td = (sensor_info*)arg;
    int sockfd = td->sockfd;
    sensor_data data;
    char buffer[BUFFER_SIZE];
    char ip[INET_ADDRSTRLEN];

    inet_ntop(AF_INET, &td->sensor_addr.sin_addr, ip, INET_ADDRSTRLEN);
    int port = ntohs(td->sensor_addr.sin_port);

    printf("[NODO CENTRALE] nuovo sensore %s:%d\n", ip ,port);

    while(1){
        ssize_t n = recv(sockfd, &data, sizeof(sensor_data), 0);
        if(n <= 0){
            if(n == 0){
                printf("Chiusura connessione\n");
            }
            perror("recv");
            break;
        }

        pthread_mutex_lock(&sensors.lock);
        int found = 0;
        
        for(int i = 0; i < sensors.counter; i++){
            if(sensors.sensors[i]->id == data.id){
                found = 1;
                sensors.sensors[i]->value = data.value;
                sensors.sensors[i]->status = data.status;
                break;
            }
        }

        if(!found && sensors.counter < MAX_SENSORS){
            sensor_info* new_sensor = malloc(sizeof(sensor_info));
            new_sensor->sockfd = sockfd;
            new_sensor->id = data.id;
            new_sensor->status = data.status;
            new_sensor->value = data.value;
            sensors.sensors[sensors.counter++] = new_sensor;
            printf("[NODO CENTRALE] nuovo Sensore-%d registrato\n", data.id);
        }

        pthread_mutex_unlock(&sensors.lock);

        if(data.value >= 75){
            printf("[NODO CENTRALE] SENSORE-%d è in stato di allarme trasmetto dai al Nodo di controllo, value: %d\n", data.id, data.value);
            snprintf(buffer, sizeof(buffer), "Allarme - ID: %d, value: %d, allarmi: %d, allarmi disattivati: %d", data.id, data.value, data.counter_allarm, data.counter_stopallarm);
            if(send(sockfd_control, buffer ,strlen(buffer), 0) < 0){
                perror("send");
                exit(EXIT_FAILURE);
            }
        }else{
            printf("[NODO CENTRALE] ricevuto dati dal SENSORE-%d, value: %d\n", data.id, data.value);
        }
    }
    close(sockfd);
} 

int main(int argc, char* argv[]){
    if(argc != 2){
        fprintf(stderr, "Usage: %s <ip_control>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    int sockfd;
    struct sockaddr_in central_addr, control_addr;

    if((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0){
        perror("socket");
        exit(EXIT_FAILURE);
    }

    memset(&central_addr, 0, sizeof(central_addr));
    central_addr.sin_family = AF_INET;
    central_addr.sin_port = htons(PORT);
    central_addr.sin_addr.s_addr = htonl(INADDR_ANY);

    if(bind(sockfd, (struct sockaddr*)&central_addr, sizeof(central_addr)) < 0){
        perror("bind");
        exit(EXIT_FAILURE);
    }

    if(listen(sockfd, MAX_SENSORS) < 0){
        perror("listen");
        exit(EXIT_FAILURE);
    }

    printf("[NODO CENTRALE] in ascolto su %d\n", PORT);

    if((sockfd_control = socket(AF_INET, SOCK_STREAM, 0)) < 0){
        perror("socket");
        exit(EXIT_FAILURE);
    }

    memset(&control_addr, 0, sizeof(control_addr));
    control_addr.sin_family = AF_INET;
    control_addr.sin_port = htons(PORT_CONTROL);
    
    if(inet_pton(AF_INET, argv[1], &control_addr.sin_addr) <= 0){
        perror("inet_pton");
        exit(EXIT_FAILURE);
    }

    if(connect(sockfd_control, (struct sockaddr*)&control_addr, sizeof(control_addr)) < 0){
        perror("connect");
        exit(EXIT_FAILURE);
    }

    printf("[NODO CENTRALE] connesso su %s:%d\n", argv[1], PORT_CONTROL);

    pthread_t control_thread;
    if(pthread_create(&control_thread, NULL, (void*)receive_control, NULL) != 0){
        perror("pthread_Create");
        exit(EXIT_FAILURE);
    }

    pthread_detach(control_thread);

    while(1){
        sensor_info* sensor = malloc(sizeof(sensor_info));
        socklen_t len = sizeof(sensor->sensor_addr);

        if((sensor->sockfd = accept(sockfd, (struct sockaddr*)&sensor->sensor_addr, &len)) < 0){
            perror("accept");
            free(sensor);
            continue;
        }

        pthread_t tid;
        if(pthread_create(&tid, NULL, (void*)handle_data, sensor) != 0){
            perror("pthread_create");
            exit(EXIT_FAILURE);
        }

        pthread_detach(tid);
    }

    pthread_mutex_destroy(&sensors.lock);
    close(sockfd_control);
    close(sockfd);
}