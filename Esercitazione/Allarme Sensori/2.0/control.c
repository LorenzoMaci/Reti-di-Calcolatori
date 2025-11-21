/* Nodo di Controllo:
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

#define BUFFER_SIZE 1024
#define PORT 6061
#define MAX_SENSOR 10

typedef struct{
    int sockfd;
    int id;
} sensor_data;

void* send_allarm(void* arg){
    sensor_data* td = (sensor_data*)arg;

    sleep(5);  // attesa di 5 secondi prima di inviare lo STOP_ALARM

    char buffer[BUFFER_SIZE];
    snprintf(buffer, sizeof(buffer), "STOP_ALARM %d", td->id);

    if(send(td->sockfd, buffer, strlen(buffer), 0) < 0){
        perror("send");
    } else {
        printf("[NODO DI CONTROLLO] invio comando di interruzione '%s' al SENSORE-%d\n", buffer, td->id);
    }

    free(td);
    pthread_exit(NULL);
}

int main(){
    int sockfd_listener;
    struct sockaddr_in control_addr;
    char buffer[BUFFER_SIZE];

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

    printf("[NODO CONTROLLO] in ascolto su porta %d\n", PORT);

    FILE* f;
    if((f = fopen("log.txt", "a")) == NULL){
        perror("fopen");
        exit(EXIT_FAILURE);
    }

    while(1){
        struct sockaddr_in central_addr;
        socklen_t len = sizeof(central_addr);
        int client_sock;

        if((client_sock = accept(sockfd_listener, (struct sockaddr*)&central_addr, &len)) < 0){
            perror("accept");
            continue;
        }

        printf("[NODO CONTROLLO] Connessione accettata dal Nodo Centrale\n");

        while(1){
            ssize_t n = recv(client_sock, buffer, BUFFER_SIZE - 1, 0);
            if(n > 0){
                buffer[n] = '\0';
                fprintf(f, "%s\n", buffer);
                printf("[NODO DI CONTROLLO] Ricevuto: %s\n", buffer);
                fflush(f);

                int id = -1;
                if(sscanf(buffer, "Allarme - Sensore: %d", &id) == 1){
                    sensor_data* td = malloc(sizeof(sensor_data));
                    td->id = id;
                    td->sockfd = client_sock;

                    pthread_t tid;
                    if(pthread_create(&tid, NULL, send_allarm, td) != 0){
                        perror("pthread_create");
                        free(td);
                        continue;
                    }
                    pthread_detach(tid);
                }
            }  else {
                perror("recv");
                close(client_sock);
                break;
            }
        }
    }

    fclose(f);
    close(sockfd_listener);
}
