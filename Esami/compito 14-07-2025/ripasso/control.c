/*
Creare 2 sensori,  in c che prendono in input un id, 
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
#define PORT 5060
#define PORT_LOG 5070
#define MAX_SENSORS 10

typedef struct{
    int id;
    int value;
    int counter_allarm;
    int counter_stopallarm;
}sensor_data;

int sockfd_log;
int sockfd_central;

void handle_allarm(void* arg){
    sensor_data* td = (sensor_data*)arg;
    char buffer[BUFFER_SIZE];

    if(td->value >= 80){
        sleep(10);
    }else{
        sleep(5);
    }

    snprintf(buffer, sizeof(buffer), "STOP_ALARM %d", td->id);

    if(send(sockfd_central, buffer, strlen(buffer), 0) < 0){
        perror("send");
        exit(EXIT_FAILURE);
    }

    free(td);
}

void handle_log(void* arg){
    sensor_data* td = (sensor_data*)arg;
    char buffer[BUFFER_SIZE];

    snprintf(buffer, sizeof(buffer), "SENSORE-%d, value: %d, allarmi: %d, stop_allarm: %d\n", td->id, td->value, td->counter_allarm, td->counter_stopallarm);

    if(send(sockfd_log, buffer, strlen(buffer), 0) < 0){
        perror("send");
        exit(EXIT_FAILURE);
    }
    free(td);
}

void handle_control(void* arg){
    char buffer[BUFFER_SIZE];    

    while(1){
        ssize_t n = recv(sockfd_central, buffer, BUFFER_SIZE - 1, 0);
        if(n <= 0){
            if(n == 0){
                printf("Chiusura connessione\n");
            }
            perror("recv");
            exit(EXIT_FAILURE);
        }

        buffer[n] = '\0';

        int id = 0, value = 0, allarm = 0, stop_allarm = 0;
        if(sscanf(buffer, "Allarme - ID: %d, value: %d, allarmi: %d, allarmi disattivati: %d",  &id, &value, &allarm, &stop_allarm) == 4){
            printf("[NODO CONTROLLO] Ricevuto: %s\n", buffer);
            sensor_data* td = malloc(sizeof(sensor_data));
            td->id = id;
            td->value = value;
            td->counter_allarm = allarm;
            td->counter_stopallarm = stop_allarm;
            
            sensor_data* td2 = malloc(sizeof(sensor_data));
            memcpy(td2, td, sizeof(sensor_data));

            pthread_t central_thread, log_thread;
            if(pthread_create(&central_thread, NULL, (void*)handle_allarm, td) != 0){
                perror("pthread_Create");
                exit(EXIT_FAILURE);
            }

           if(pthread_create(&log_thread, NULL, (void*)handle_log, td2) != 0){
                perror("pthread_Create");
                exit(EXIT_FAILURE);
           } 

            pthread_detach(central_thread);
            pthread_detach(log_thread);
        }
    }
}

int main(int argc, char* argv[]){
    if(argc != 2){
        fprintf(stderr, "Usage: %s <ip_log>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    int sockfd;
    struct sockaddr_in control_addr, log_addr;
    if((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0){
        perror("socket");
        exit(EXIT_FAILURE);
    }

    memset(&control_addr, 0, sizeof(control_addr));
    control_addr.sin_family = AF_INET;
    control_addr.sin_port = htons(PORT);
    control_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    
    if(bind(sockfd, (struct sockaddr*)&control_addr, sizeof(control_addr)) < 0){
        perror("bind");
        exit(EXIT_FAILURE);
    }

    if(listen(sockfd, MAX_SENSORS) < 0){
        perror("listen");
        exit(EXIT_FAILURE);
    }

    printf("[NODO CONTROLLO] in ascolto su %d\n", PORT);

    if((sockfd_log = socket(AF_INET, SOCK_STREAM, 0)) < 0){ 
        perror("socket");
        exit(EXIT_FAILURE);
    }

    memset(&log_addr, 0, sizeof(log_addr));
    log_addr.sin_family = AF_INET;
    log_addr.sin_port = htons(PORT_LOG);
    
    if(inet_pton(AF_INET, argv[1],  &log_addr.sin_addr) <= 0){
        perror("inet_pton");
        exit(EXIT_FAILURE);
    }

    if(connect(sockfd_log, (struct sockaddr*)&log_addr, sizeof(log_addr)) < 0){
        perror("connect");
        exit(EXIT_FAILURE);
    }

    printf("[NODO CONTROLLO] connesso su %s:%d\n", argv[1], PORT_LOG);

    struct sockaddr_in from_addr;
    socklen_t len = sizeof(from_addr);

    if((sockfd_central = accept(sockfd, (struct sockaddr*)&from_addr, &len)) < 0){
        perror("accept");
        close(sockfd);
        exit(EXIT_FAILURE);
    }

    pthread_t tid;
    if(pthread_create(&tid, NULL, (void*)handle_control, NULL) != 0){
        perror("pthread_Create");
        exit(EXIT_FAILURE);
    }

    pthread_join(tid, NULL);

    close(sockfd);
    close(sockfd_central);
   close(sockfd_log);
}
