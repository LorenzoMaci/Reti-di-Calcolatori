/*Nodo Centrale:
Memorizza un nuovo sensore quando riceve un messaggio da esso
Se la temperatura è superiore a 30°C o la qualità dell'aria è scarsa, deve inviare un messaggio al Nodo di Controllo
La comunicazione deve essere affidabile
Se il Nodo di Controllo invia un comando di arresto allarme al nodo specifico, il Nodo Centrale invia questo comando al Nodo Sensore specifico
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
#define PORT_CONTROL 7777
#define MAX_SENSOR 10

typedef struct{
    int temperatura;
    int umidity;
    int quality_of_air;
    int status;
    int id;
}sensor_data;

typedef struct{
    int sockfd;
    struct sockaddr_in sensor_addr;
    int temperatura;
    int umidity;
    int quality_of_air;
    int status;
    int id;
}sensor_info;

typedef struct{
    sensor_info* sensors[MAX_SENSOR];
    int counter;
    pthread_mutex_t lock;
}sensor_list;

sensor_list sensors = {.counter = 0, .lock = PTHREAD_MUTEX_INITIALIZER};
int sockfd_control;

void send_stopallarm(sensor_info* td){
    char buffer[BUFFER_SIZE];
    snprintf(buffer, sizeof(buffer), "STOP_ALARM");

    if(send(td->sockfd, buffer, strlen(buffer), 0) < 0){
        perror("send");
        exit(EXIT_FAILURE);
    }

    printf("[NODO CENTRALE] Invio comando di arresto '%s' al Sensore-%d\n", buffer, td->id);
}

void receive_handle(void* arg){
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

        if(!strncmp(buffer, "STOP_ALARM", 10)){
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
                    send_stopallarm(found);
                }else{
                    printf("[NODO CENTRALE] Sensore %d non trovato\n", id);
                }
            }
        }
    }
}


void handle_sensor(void* arg){
    sensor_info* td = (sensor_info*)arg;
    int sockfd = td->sockfd;
    char buffer[BUFFER_SIZE];
    char sensor_ip[INET_ADDRSTRLEN];
    sensor_data data;
    
    inet_ntop(AF_INET, &td->sensor_addr.sin_addr, sensor_ip, INET_ADDRSTRLEN);
    int port = ntohs(td->sensor_addr.sin_port);

    printf("[NODO CENTRALE] Nuova Connessione da %s:%d\n", sensor_ip, port);

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
                sensors.sensors[i]->temperatura = data.temperatura;
                sensors.sensors[i]->umidity = data.umidity;
                sensors.sensors[i]->quality_of_air = data.quality_of_air;
                sensors.sensors[i]->status = data.status;
                break;
            }
        }
        

        if(!found && sensors.counter < MAX_SENSOR){
            sensor_info* new_sensor = malloc(sizeof(sensor_info));
            new_sensor->id = data.id;
            new_sensor->temperatura = data.temperatura;
            new_sensor->umidity = data.umidity;
            new_sensor->quality_of_air = data.quality_of_air;
            new_sensor->status = data.status;
            new_sensor->sockfd = sockfd;
            sensors.sensors[sensors.counter++] = new_sensor;
            printf("[NODO CENTRALE] Sensore-%d aggiunto\n", data.id);
        }

        pthread_mutex_unlock(&sensors.lock);

        printf("[NODO CENTRALE] ricevuti dati dal Sensore-%d: temperatura: %d, umidità: %d, qualità dell'aria: %d\n", data.id, data.temperatura, data.umidity, data.quality_of_air);

        if(data.temperatura >= 30 || data.quality_of_air >= 66){
            printf("[NODO CENTRALE] Invio allarme al nodo di controllo\n");
            snprintf(buffer, sizeof(buffer), "Allarme - Sensore %d, temperatura: %d, umidità: %d, qualità dell'aria: %d\n", data.id, data.temperatura, data.umidity, data.quality_of_air);
            if(send(sockfd_control, buffer, strlen(buffer), 0) < 0){
                perror("send");
                exit(EXIT_FAILURE);
            }
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

    if(listen(sockfd, MAX_SENSOR) < 0){
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

    printf("[NODO CENTRALE] Connesso al nodo di Controllo su %s:%d\n", argv[1], PORT_CONTROL);

    pthread_t control_thread;

    if(pthread_create(&control_thread, NULL, (void*)receive_handle, NULL) != 0){
        perror("pthread_create");
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
        
        if(pthread_create(&tid, NULL, (void*)handle_sensor, sensor) != 0){
            perror("pthread_create");
            close(sensor->sockfd);
            free(sensor);
        }

        pthread_detach(tid);
    }

    close(sockfd_control);
    close(sockfd);
}