/* Creare 2 sensori,  in c  che prendono in input un id, ogni 5 sec generavano un numero casuale  tra 63 e 97 e lo inviavano ad un central node. 
Se il numero era maggiore di 75 entra in uno stato di allarme e smette di inviare messaggi fino a quando non viene sbloccato.
- Il central node riceve le misurazioni dei sensori e se maggiori di 75 trasmette id e misurazione ad un control node.
- Il control node riceve le misurazioni e se il valore è <80 aspetta 5 sec ed invia un messaggio di sblocco del sensore al central node 
  che poi lo girerà al sensore, se >80 aspetterà 10 sec prima di sbloccare il sensore.
- Infine un log node che registra in un file tutte le misurazioni di tutti i sensori, gli allarmi e gli sblocchi*/

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
#define PORT_RECV 9091
#define PORT 9092
#define MAX_SENSOR 10
#define LOG_PORT 9093

typedef struct{
    int id;
    int value;
    int status;
    int counter_allarm;
    int counter_disableallarm;
}sensor_data;

char ip_central[INET_ADDRSTRLEN];
char ip_log[INET_ADDRSTRLEN];

void send_allarm(void* arg){
    sensor_data* td = (sensor_data*)arg;
    int sockfd;
    struct sockaddr_in central_addr;
    char buffer[BUFFER_SIZE];

    if((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0){
        perror("sockfd");
        exit(EXIT_FAILURE);
    }

    memset(&central_addr, 0, sizeof(central_addr));
    central_addr.sin_family = AF_INET;
    central_addr.sin_port = htons(PORT);

    if(inet_pton(AF_INET, ip_central, &central_addr.sin_addr) <= 0){
        perror("inet_pton");
        exit(EXIT_FAILURE);
    }

    if(connect(sockfd, (struct sockaddr*)&central_addr, sizeof(central_addr)) < 0){
        perror("connect");
        exit(EXIT_FAILURE);
    }

    if(td->value < 80){
        sleep(5);
        snprintf(buffer, sizeof(buffer), "STOP_ALARM %d", td->id);
    }else{
        sleep(10);
        snprintf(buffer, sizeof(buffer), "STOP_ALARM %d", td->id);
    }

    printf("[Nodo di Controllo] inviato comando di interruzione: %s\n", buffer);

    if(send(sockfd, buffer, strlen(buffer), 0) < 0){
        perror("send");
        exit(EXIT_FAILURE);
    }
    close(sockfd);
    free(td);
}

void send_log(void* arg){
    sensor_data* td = (sensor_data*)arg;
    int sockfd;
    struct sockaddr_in log_addr;
    char buffer[BUFFER_SIZE];

    if((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0){
        perror("socket");
        exit(EXIT_FAILURE);
    }

    memset(&log_addr, 0, sizeof(log_addr));
    log_addr.sin_family = AF_INET;
    log_addr.sin_port = htons(LOG_PORT);

    if(inet_pton(AF_INET, ip_log, &log_addr.sin_addr) <= 0){
        perror("inet_pton");
        exit(EXIT_FAILURE);
    }

    if(connect(sockfd, (struct sockaddr*)&log_addr, sizeof(log_addr)) < 0){
        perror("connect");
        exit(EXIT_FAILURE);
    }

    snprintf(buffer, sizeof(buffer), "Sensore ID: %d, value: %d, allarm: %d, disable: %d\n", td->id, td->value, td->counter_allarm, td->counter_disableallarm);

    printf("[NODO CONTROLLO]: Invio dati al log_node %s\n", buffer);

    if(send(sockfd, buffer, strlen(buffer), 0) < 0){
        perror("send");
        pthread_exit(NULL);
    }
    close(sockfd);
    free(td);
}

int main(int argc, char* argv[]){
    if(argc != 4){
        fprintf(stderr, "Usage: %s <IP_Control> <Ip_central> <IP_log>\n" ,argv[0]);
        exit(EXIT_FAILURE);
    }

    int sockfd;
    struct sockaddr_in control_addr;
    strcpy(ip_central, argv[2]);
    strcpy(ip_log, argv[3]);


    if((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0){
        perror("sockfd");
        exit(EXIT_FAILURE);
    }

    memset(&control_addr, 0, sizeof(control_addr));
    control_addr.sin_family = AF_INET;
    control_addr.sin_port = htons(PORT_RECV);

    if(inet_pton(AF_INET, argv[1], &control_addr.sin_addr) <= 0){
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


    printf("[NODO CONTROLLO]: in ascolto su %s:%d\n", argv[1], PORT_RECV);

    char buffer[BUFFER_SIZE];

    while(1){
        int clientfd;
        struct sockaddr_in sensor_addr;
        socklen_t len = sizeof(sensor_addr);
        
        if((clientfd = accept(sockfd, (struct sockaddr*)&sensor_addr, &len)) < 0){
            perror("accept");
            exit(EXIT_FAILURE);
        }

        ssize_t n = recv(clientfd, buffer, BUFFER_SIZE - 1,  0);
        if(n <= 0){
            if(n == 0){
                printf("Connessione Chiusa\n");
            }
            perror("recv");
            exit(EXIT_FAILURE);
        }

        buffer[n] = '\0';
        printf("[NODO CONTROLLO]: Ricevuto: %s\n", buffer);

        int id = 0,  value = 0, counter_allarm = 0, counter_disableallarm = 0;
        if(sscanf(buffer, "Sensore ID: %d, value: %d, allarm: %d, disable: %d", &id, &value, &counter_allarm, &counter_disableallarm) == 4){
            sensor_data* td = malloc(sizeof(sensor_data));
            td->id = id;
            td->value = value;
            td->counter_allarm = counter_allarm;
            td->counter_disableallarm = counter_disableallarm;

            sensor_data* td2 = malloc(sizeof(sensor_data));
            memcpy(td2, td, sizeof(sensor_data));

            pthread_t sensor_allarm, log_message;
            if(pthread_create(&sensor_allarm, NULL, (void*)send_allarm, td) != 0){
                perror("pthread_Create");
                exit(EXIT_FAILURE);
            }

            if(pthread_create(&log_message, NULL, (void*)send_log, td2) != 0){
                perror("pthread_create");
                exit(EXIT_FAILURE);
            }

            pthread_detach(sensor_allarm);
            pthread_detach(log_message);
        }
        close(clientfd);   
    }
    close(sockfd);
}
