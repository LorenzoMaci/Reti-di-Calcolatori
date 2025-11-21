/* Creare 2 sensori,  in c che prendono in input un id, 
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
#define SEND_PORT 9089
#define RECV_PORT 9090 //porta di ascolto  per il sensor node
#define PORT 9091 //porta di invio messaggi dal control node
#define PORT_RECV 9092 //Porta di ascolto per il control node
#define MAX_SENSOR 10

typedef struct{
    int id;
    int value;
    int status;
    int counter_allarm;
    int counter_disableallarm;
}sensor_data;

typedef struct{
    int sockfd;
    struct sockaddr_in addr;
    int id;
    char ip[INET_ADDRSTRLEN];
}sensor_info;

typedef struct{
    sensor_info* sensors[MAX_SENSOR];
    int counter;
    pthread_mutex_t lock;
}sensor_list;

sensor_list sensors = {.counter = 0, .lock = PTHREAD_MUTEX_INITIALIZER};
char ip_control[INET_ADDRSTRLEN];
char ip_central[INET_ADDRSTRLEN];

void send_control(sensor_data* sensor){
    int sockfd;
    char buffer[BUFFER_SIZE];
    struct sockaddr_in control_addr;

    if((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0){
        perror("socket");
        exit(EXIT_FAILURE);
    }

    memset(&control_addr, 0, sizeof(control_addr));
    control_addr.sin_family = AF_INET;
    control_addr.sin_port = htons(PORT);

    if(inet_pton(AF_INET, ip_control, &control_addr.sin_addr) <= 0){
        perror("inet_pton");
        exit(EXIT_FAILURE);
    }

    if(connect(sockfd, (struct sockaddr*)&control_addr, sizeof(control_addr)) < 0){
        perror("connect");
        exit(EXIT_FAILURE);
    }

    snprintf(buffer, sizeof(buffer), "Sensore ID: %d, value: %d, allarm: %d, disable: %d\n", sensor->id, sensor->value, sensor->counter_allarm, sensor->counter_disableallarm);

    if(send(sockfd, buffer, strlen(buffer), 0) < 0){
        perror("send");
        exit(EXIT_FAILURE);
    }
    
    printf("[Nodo Centrale] invio allarme al nodo di controllo\n");
    close(sockfd);
}

void handle_sensor(void* arg){
    sensor_info* td = (sensor_info*)arg;
    int sock_sensor = td->sockfd;
    char ip[INET_ADDRSTRLEN];

    inet_ntop(AF_INET, &td->addr.sin_addr, ip, INET_ADDRSTRLEN);
    int port = ntohs(td->addr.sin_port);

    printf("[Nodo Centrale] Nuova connessione del Sensore %s:%d\n", ip, port);

    while(1){
        sensor_data sensor;
        ssize_t n = recv(sock_sensor, &sensor, sizeof(sensor_data), 0);
        if(n <= 0){
            if(n == 0){
                printf("Connessione chiusa\n");
            }
            perror("recv");
            exit(EXIT_FAILURE);
        }

        pthread_mutex_lock(&sensors.lock);

        int found = 0;
        for(int i = 0; i < sensors.counter; i++){
            if(sensors.sensors[i]->id == sensor.id){
                found = 1;
                break;
            }
        }
        

        if(!found && sensors.counter < MAX_SENSOR){
            sensor_info* new_sensor = malloc(sizeof(sensor_info));
            new_sensor->id = sensor.id;
            new_sensor->sockfd = sock_sensor;
            strncpy(new_sensor->ip, ip, INET_ADDRSTRLEN);
            sensors.sensors[sensors.counter++] = new_sensor;
            printf("[NODO CENTRALE]: Nodo Sensore registrato con ID: %d\n", sensor.id);
        }

        pthread_mutex_unlock(&sensors.lock);


        printf("[NODO CENTRALE]: Ricevuto dati dal Nodo Sensore: %d, value: %d\n", sensor.id, sensor.value);

        if(sensor.value >= 75){
            send_control(&sensor);           
        }
    }
    close(sock_sensor); 
}

void sendsensor_allarm(sensor_info* sensor){
    char buffer[BUFFER_SIZE];
    snprintf(buffer, sizeof(buffer), "STOP_ALARM");

    if(send(sensor->sockfd, buffer, strlen(buffer),  0) < 0){
        perror("send");
        exit(EXIT_FAILURE);
    }

    printf("[Nodo Centrale] Comando di arresto allarme inviato al sensore ID: %d\n", sensor->id);
}   

void receive_handle(void* arg){
    int sockfd;
    struct sockaddr_in control_addr;
    char buffer[BUFFER_SIZE];

    if((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0){
        perror("sockfd");
        exit(EXIT_FAILURE);
    }

    memset(&control_addr, 0, sizeof(control_addr));
    control_addr.sin_family = AF_INET;
    control_addr.sin_port = htons(PORT_RECV);
    
    if(inet_pton(AF_INET, ip_central, &control_addr.sin_addr) <= 0){
        perror("inet_pton");
        exit(EXIT_FAILURE);
    }

    if(bind(sockfd, (struct sockaddr*)&control_addr, sizeof(control_addr)) < 0){
        perror("bind");
        exit(EXIT_FAILURE);
    }

    if(listen(sockfd, MAX_SENSOR) < 0){
        perror("listen");
        exit(EXIT_FAILURE);
    }

    printf("[Nodo Centrale] In attesa di comandi dal Nodo di Controllo su porta %d\n", RECV_PORT);
    
    while(1){
        int clienfd;
        struct sockaddr_in client_addr;
        socklen_t len = sizeof(client_addr);
        if((clienfd = accept(sockfd, (struct sockaddr*)&client_addr, &len)) < 0){
            perror("accept");
            exit(EXIT_FAILURE);
        }

        ssize_t n = recv(clienfd, buffer, BUFFER_SIZE - 1, 0);
        if(n <= 0){
            if(n == 0){
                printf("connessione chiusa dal Nodo di controllo\n");
            }
            perror("recv");
            exit(EXIT_FAILURE);
        }

        buffer[n] = '\0';

        int sensor_id;
        if(sscanf(buffer, "STOP_ALARM %d", &sensor_id) == 1){
            sensor_info* found = NULL;            
            pthread_mutex_lock(&sensors.lock);
            for(int i = 0; i < sensors.counter; i++){
                if(sensors.sensors[i]->id == sensor_id){
                    found = sensors.sensors[i];
                    break;
                }
            }

            pthread_mutex_unlock(&sensors.lock);

            if(found != NULL){
                sendsensor_allarm(found);
            }else{
                printf("[NODO CENTRALE] Sensore %d non trovato\n", sensor_id);
            }
        }
        close(clienfd);
    }
    close(sockfd);
}

int main(int argc, char* argv[]){
    if(argc != 3){
        fprintf(stderr, "Usage: %s <IP_Central> <IP_Control>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    strcpy(ip_control, argv[2]);
    strcpy(ip_central, argv[1]);

    int sockfd;
    struct sockaddr_in sensor_addr;

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

    printf("[NODO CENTRALE]: in ascolto su Porta %d\n", RECV_PORT);

    pthread_t control_thread;
    if(pthread_create(&control_thread, NULL, (void*)receive_handle, NULL) < 0){
        perror("pthread_create");
        exit(EXIT_FAILURE);
    }

    pthread_detach(control_thread);

    while(1){
        sensor_info* info = malloc(sizeof(sensor_info));
        socklen_t len = sizeof(info->addr);

        if((info->sockfd = accept(sockfd, (struct sockaddr*)&info->addr, &len)) < 0){
            perror("accept");
            free(info);
            exit(EXIT_FAILURE);
        }

        pthread_t sensor_thread;
        if(pthread_create(&sensor_thread, NULL, (void*)handle_sensor, info) < 0){
            perror("pthread_create");
            free(info);
            exit(EXIT_FAILURE);
        }

        pthread_detach(sensor_thread);
    }
    close(sockfd);
}   