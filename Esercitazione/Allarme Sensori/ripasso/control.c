/*Nodo di Controllo:
Stampa gli Allarmi
Memorizza un file di LOG di tutti gli allarmi
Dopo 5 secondi, se un Nodo Sensore entra in modalità Allarme, la interromperà
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
#define PORT 7777
#define MAX_SENSOR 10

typedef struct{
    int id;
}allarm;

int sockfd;

void handle_allarm(void* arg){
    allarm* td = (allarm*)arg;
    char buffer[BUFFER_SIZE];

    sleep(5);

    snprintf(buffer, sizeof(buffer), "STOP_ALARM %d", td->id);

    printf("[NDOO CONTROLLO] Invio comando di interruzione al Nodo Centrale per il Sensore-%d\n", td->id);

    if(send(sockfd, buffer, strlen(buffer), 0) < 0){
        perror("send");
        exit(EXIT_FAILURE);
    }

    free(td);
    pthread_exit(NULL);
}

int main(){
    int sockfd_listener;
    struct sockaddr_in control_addr;

    if((sockfd_listener = socket(AF_INET, SOCK_STREAM, 0)) < 0){
        perror("socket");
        exit(EXIT_FAILURE);
    }

    memset(&control_addr, 0, sizeof(control_addr));
    control_addr.sin_family = AF_INET;
    control_addr.sin_port = htons(PORT);
    control_addr.sin_addr.s_addr = htonl(INADDR_ANY);

    if(bind(sockfd_listener, (struct sockaddr*)&control_addr, sizeof(control_addr)) < 0){
        perror("bind");
        exit(EXIT_FAILURE);
    }

    if(listen(sockfd_listener, MAX_SENSOR) < 0){
        perror("listen");
        exit(EXIT_FAILURE);
    }

    printf("[NODO CONTROLLO] in ascolto su %d\n", PORT);

    FILE* f;

    if((f = fopen("allarm.log", "a")) == NULL){
        perror("fopen");
        exit(EXIT_FAILURE);
    }

    char buffer[BUFFER_SIZE];
    struct sockaddr_in central_addr;
    socklen_t len = sizeof(control_addr);

    if((sockfd = accept(sockfd_listener, (struct sockaddr*)&central_addr, &len)) < 0){
        perror("accept");
        close(sockfd_listener);
        exit(EXIT_FAILURE);
    }

    while(1){
        ssize_t n = recv(sockfd, buffer, BUFFER_SIZE - 1, 0);

        if(n <= 0){
            if(n == 0){
                printf("Chiusura connessione\n");
            }
            perror("recv");
            break;
        }

        buffer[n] = '\0';
        printf("[NODO DI CONTROLLO] Ricevuto: %s\n", buffer);
        fprintf(f, "%s\n", buffer);
        fflush(f);

        int id = -1;
        if(sscanf(buffer, "Allarme - Sensore %d", &id) == 1){
            allarm* td = malloc(sizeof(allarm));
            td->id = id;

            pthread_t tid;
            if(pthread_create(&tid, NULL, (void*)handle_allarm, td) != 0){
                perror("pthread_Create");
                free(td);
                exit(EXIT_FAILURE);
            }
            pthread_detach(tid);
        } 
    }

    fclose(f);
    close(sockfd);
    close(sockfd_listener);
}

