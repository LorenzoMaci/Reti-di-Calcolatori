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


#define BUFFER_SIZE 1024
#define RECV_SENSOR 9090
#define PORT 9091
#define RECV_CONTROL 9092
#define SEND_PORT 9093
#define MAX_SENSOR 100

typedef struct{
    float temperatura;
    float quality_air;
    float umidity;
    int id;
    int status;
    int port;
}sensor_data;

typedef struct{
    int id;
    struct sockaddr_in addr;
    int port;
}sensor_info;

typedef struct{
    sensor_info* sensor[MAX_SENSOR];
    int counter;
    pthread_mutex_t lock;
}sensor_list;

typedef struct{
    int sockfd;
    struct sockaddr_in client_addr;
}thread_sensor;

sensor_list sensor = {.counter = 0, .lock = PTHREAD_MUTEX_INITIALIZER};
char ip_control[INET_ADDRSTRLEN];
char ip_central[INET_ADDRSTRLEN];

void send_allarm(sensor_data* td){
    int sockfd;
    struct sockaddr_in control_addr;
    char buffer[BUFFER_SIZE];

    if((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0){
        perror("socket");
        exit(EXIT_FAILURE);
    }

    memset(&control_addr, 0 , sizeof(control_addr));
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

    snprintf(buffer, sizeof(buffer), "Allarme dal sensore ID: %d, Temperatura: %.2f, Umidità: %.2f, Qualita dell'aria: %.2f\n", td->id, td->temperatura, td->umidity, td->quality_air);

    if(send(sockfd, buffer, strlen(buffer), 0) < 0){
        perror("send");
        exit(EXIT_FAILURE);
    }
    
    printf("[Nodo Centrale] Allarme inviato al Nodo di controllo\n");
    
    close(sockfd);
}

void send_stop_to_sensor(sensor_info* td){
    int sockfd;
    char buffer[BUFFER_SIZE];

    if((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0){
        perror("socket");
        exit(EXIT_FAILURE);
    }

    td->addr.sin_port = htons(td->port);

    if(connect(sockfd, (struct sockaddr*)&td->addr, sizeof(td->addr)) < 0){
        perror("connect");
        close(sockfd);
        return;
    }   

    snprintf(buffer, sizeof(buffer), "STOP_ALARM %d", td->id);
    
    if(send(sockfd, buffer, strlen(buffer), 0) < 0){
        perror("send");
        exit(EXIT_FAILURE);
    }
        
    printf("[Nodo Centrale] Comando di arresto allarme inviato al sensore ID: %d\n", td->id);
    
    close(sockfd);
}


void control_listener(void* arg){
    int sockfd, client_sockfd;
    struct sockaddr_in server_addr, client_addr;
    socklen_t addr_len = sizeof(client_addr);
    char buffer[BUFFER_SIZE];

    if((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0){
        perror("socket");
        exit(EXIT_FAILURE);
    }
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(RECV_CONTROL);

    if(inet_pton(AF_INET, ip_central, &server_addr.sin_addr) <= 0){
        perror("inet_pton");
        exit(EXIT_FAILURE);
    }

    if(bind(sockfd, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0){
        perror("bind");
        exit(EXIT_FAILURE);
    }

    if(listen(sockfd, 5) < 0){
        perror("listen");
        exit(EXIT_FAILURE);
    }

    printf("[Nodo Centrale] In attesa di comandi dal Nodo di Controllo...\n");
    while(1){
        if((client_sockfd = accept(sockfd, (struct sockaddr*)&client_addr, &addr_len)) < 0){
            perror("accept");
            continue;
        }

        ssize_t n = recv(client_sockfd, buffer, BUFFER_SIZE - 1, 0);
        if(n <= 0){
            perror("recv");
            close(client_sockfd);
            continue;
        }

        buffer[n] = '\0';
        int sensor_id;
        if(sscanf(buffer, "STOP_ALARM %d", &sensor_id) == 1){
            sensor_info* sensor_found = NULL;
            if(pthread_mutex_lock(&sensor.lock) != 0){
                perror("pthread_mutex_lock");
                exit(EXIT_FAILURE);
            }
            for(int i = 0; i < sensor.counter; i++){
                if(sensor.sensor[i]->id == sensor_id){
                    sensor_found = sensor.sensor[i];
                    break;
                }
            }
            if(pthread_mutex_unlock(&sensor.lock) != 0){
                perror("pthread_mutex_unlock");
                exit(EXIT_FAILURE);
            }

            if(sensor_found){
                send_stop_to_sensor(sensor_found);
            }else{
                printf("[Nodo Centrale] Sensore ID: %d non trovato\n", sensor_id);
            }
        }
        close(client_sockfd);
    }
    close(sockfd);
}

void sensor_handler(void* arg){
    thread_sensor* td = (thread_sensor*)arg;
    int client_sockfd = td->sockfd;
    struct sockaddr_in client_addr = td->client_addr;

    while(1){
        sensor_data data;
        ssize_t n = recv(client_sockfd, &data, sizeof(sensor_data), 0);
        if(n <= 0){
            if(n == 0){
                printf("[Nodo Centrale] Connessione chiusa dal sensore ID: %d\n", data.id);
            }else{
                perror("recv");
            }
            close(client_sockfd);
        }

        if(pthread_mutex_lock(&sensor.lock) != 0){
            perror("pthread_mutex_lock");
            exit(EXIT_FAILURE);
        }

        int found = 0;
        for(int i = 0; i < sensor.counter; i++){
            if(sensor.sensor[i]->id == data.id){
                found = 1;
                break;
            }
        }

        if(!found && sensor.counter < MAX_SENSOR){
            sensor_info* new_sensor = malloc(sizeof(sensor_info));
            new_sensor->id = data.id;
            new_sensor->addr = client_addr;
            new_sensor->port = data.port;
            sensor.sensor[sensor.counter++] = new_sensor;
            printf("[Nodo Centrale] Nuovo sensore registrato ID: %d, con IP: %s e Porta: %d\n", data.id, inet_ntoa(new_sensor->addr.sin_addr), new_sensor->port);
        }

        if(pthread_mutex_unlock(&sensor.lock) != 0){
            perror("pthread_mutex_unlock");
            exit(EXIT_FAILURE);
        }

        printf("[Nodo Centrale] Ricevuto dati dal sensore ID: %d, Temperatura: %.2f, Umidità: %.2f, Qualità dell'aria: %.2f\n", data.id, data.temperatura, data.umidity, data.quality_air);

        if(data.temperatura > 30.0 || data.quality_air >= 66.0){
            send_allarm(&data);
        }
    }
    close(client_sockfd);
    free(td);
}

int main(int argc, char* argv[]){
    if(argc != 3){
        fprintf(stderr, "Usage: %s <IP-Centrale> <IP-Controllo>\n", argv[0]);
        exit(EXIT_FAILURE);
    }
    
    strncpy(ip_control, argv[2], INET_ADDRSTRLEN);
    strncpy(ip_central, argv[1], INET_ADDRSTRLEN);
    int sockfd, client_sockfd;
    struct sockaddr_in server_addr, client_addr;
    socklen_t addr_len = sizeof(client_addr);
    
    if((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0){
        perror("socket");
        exit(EXIT_FAILURE);
    }
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(RECV_SENSOR);

    if(inet_pton(AF_INET, argv[1], &server_addr.sin_addr) <= 0){
        perror("invalid address");
        exit(EXIT_FAILURE);
    }
    if(bind(sockfd, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0){
        perror("bind");
        exit(EXIT_FAILURE);
    }
    if(listen(sockfd, 10) < 0){
        perror("listen");
        exit(EXIT_FAILURE);
    }

    printf("[Nodo Centrale] in ascolto su %s:%d\n", argv[1], RECV_SENSOR);

    pthread_t control_thread;
    if(pthread_create(&control_thread, NULL, (void*)control_listener, NULL) != 0){
        perror("pthread_create");
        exit(EXIT_FAILURE);
    }

    if (pthread_detach(control_thread) != 0) {
        perror("pthread_detach");
    }

    while(1){
        if((client_sockfd = accept(sockfd, (struct sockaddr*)&client_addr, &addr_len)) < 0){
            perror("accept");
            continue;
        }
        
        thread_sensor* td = malloc(sizeof(thread_sensor));
        td->sockfd = client_sockfd;
        td->client_addr = client_addr;

        pthread_t sensor_thread;
        if(pthread_create(&sensor_thread, NULL, (void*)sensor_handler, td) != 0){
            perror("pthread_create");
            close(client_sockfd);
            free(td);
            continue;
        }

        pthread_detach(sensor_thread);
        printf("[Nodo Centrale] Nuova connessione da %s:%d\n", inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));
    }
    close(sockfd);
}
