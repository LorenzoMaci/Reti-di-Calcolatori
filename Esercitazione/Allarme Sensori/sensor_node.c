/*Nodo Sensore:
Può inviare dati ogni 3 secondi
a comunicazione deve essere affidabile
I dati del sensore possono essere temperatura, umidità e qualità dell'aria
Ogni sensore deve essere identificato da un ID univoco
Se la temperatura è superiore a 30°C o la qualità dell'aria è scarsa, deve entrare in modalità Allarme
In modalità Allarme non è possibile inviare dati del sensore
Quando riceve un comando per interrompere la modalità allarme, tornerà in modalità normale
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <time.h> // Per generare dati casuali

#define BUFFER_SIZE 1024
#define PORT 9090 //porta d'invio messaggi

typedef struct{
    float temperatura;
    float quality_air;
    float umidty;
    int id;
    int status;
    int port;
}sensor_data;

int sockfd; //socket di invio messaggi
int recv_sockfd; //socket di ricezione messaggi 
pthread_mutex_t lock;

void send_data(void* arg){
    sensor_data* td = (sensor_data*)arg;

    while(1){
        sleep(3);
        if(pthread_mutex_lock(&lock) != 0){
            perror("pthread_mutex_lock");
            exit(EXIT_FAILURE);
        }

        td->quality_air = rand()%100;
        td->umidty = rand()%100;
        td->temperatura = rand()%48;

        if(td->status == 1){
            printf("[Sensore %d] in stato di Allarme invio sospeso\n", td->id);
            if(pthread_mutex_unlock(&lock) != 0){
                perror("pthread_mutex_unlock");
                exit(EXIT_FAILURE);
            }
            continue;    
        }


        if(td->temperatura > 30.0 || td->quality_air >= 66.0){
            printf("[Sensore %d] Entro in stato di Allarme\n", td->id);
            printf("[Sensore %d] Invio Dati: Temperatura: %.2f, Umidità = %.2f, Qualità dell'aria: %.2f\n", td->id, td->temperatura, td->umidty, td->quality_air);
            if(send(sockfd, td, sizeof(sensor_data), 0 ) < 0){
                perror("send");
                exit(EXIT_FAILURE);
            }
            td->status = 1; // Modalità Allarme
            if(pthread_mutex_unlock(&lock) != 0){
                perror("pthread_mutex_unlock");
                exit(EXIT_FAILURE);
            }
            continue;
        }
        
        printf("[Sensore %d] Invio Dati in modalità normale\n", td->id);
        printf("[Sensore %d] Invio Dati: Temperatura: %.2f, Umidità = %.2f, Qualità dell'aria: %.2f\n", td->id, td->temperatura, td->umidty, td->quality_air);
        
        if(send(sockfd, td, sizeof(sensor_data), 0) < 0){
            perror("send");
            exit(EXIT_FAILURE);
        }

        if(pthread_mutex_unlock(&lock) != 0){
            perror("pthread_mutex_unlock");
            exit(EXIT_FAILURE);
        }

    }
}

void receive_handle(void* arg){
    sensor_data* td = (sensor_data*)arg;
    char buffer[BUFFER_SIZE];
    int client_sockfd;
    struct sockaddr_in client_addr;
    socklen_t client_len = sizeof(client_addr);


    while(1){
        if((client_sockfd = accept(recv_sockfd, (struct sockaddr*)&client_addr, &client_len)) < 0){
            perror("accept");
            exit(EXIT_FAILURE);
        }
        
        ssize_t n = recv(client_sockfd, buffer, BUFFER_SIZE - 1, 0);
        if(n <= 0){
            perror("Connessione dal server chiusa / errore");
            exit(EXIT_FAILURE);
        }

        buffer[n] = '\0';

        if(!strncmp(buffer, "STOP_ALARM", 10)){
            if(pthread_mutex_lock(&lock) != 0){
                perror("pthread_mutex_lock");
                exit(EXIT_FAILURE);
            }

            td->status = 0;
            printf("[Sensore %d] Allarme disattivato dal Nodo Centrale\n", td->id);

            if(pthread_mutex_unlock(&lock) != 0){
                perror("pthread_mutex_unlock");
                exit(EXIT_FAILURE);
            }
        }
        close(client_sockfd);
    }
}

int main(int argc, char* argv[]){
    if(argc != 5){
        fprintf(stderr, "Usage: %s <IP_Sensore> <IP_Centrale> <ID> <RECV_PORT>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    srand(time(NULL)); 
    struct sockaddr_in server_addr, recv_addr;
    sensor_data sensor;
    sensor.id = atoi(argv[3]);
    sensor.status = 0;
    sensor.port = atoi(argv[4]);

    if(pthread_mutex_init(&lock, NULL) != 0){
        perror("Pthread_mutex-init");
        exit(EXIT_FAILURE);
    }

    if((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0){
        perror("sockfd");
        exit(EXIT_FAILURE);
    }

    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(PORT);

    if(inet_pton(AF_INET, argv[2], &server_addr.sin_addr) <= 0){
        perror("invalid address");
        exit(EXIT_FAILURE);
    }

    if(connect(sockfd, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0){
        perror("Connection failed");
        exit(EXIT_FAILURE);
    }

    printf("Connesso al Nodo Centrale %s : %d\n", argv[2], PORT);

    if((recv_sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0){
        perror("recv_sockfd");
        exit(EXIT_FAILURE);
    }

    memset(&recv_addr, 0, sizeof(recv_addr));
    recv_addr.sin_family = AF_INET;
    recv_addr.sin_port = htons(sensor.port);
    if(inet_pton(AF_INET, argv[1], &recv_addr.sin_addr) <= 0){
        perror("invalid address");
        exit(EXIT_FAILURE);
    }

    if(bind(recv_sockfd, (struct sockaddr*)&recv_addr, sizeof(recv_addr)) < 0){
        perror("bind");
        exit(EXIT_FAILURE);
    }

    if(listen(recv_sockfd, 5) < 0){
        perror("listen");
        exit(EXIT_FAILURE);
    }

    printf("In ascolto su %s : %s\n", argv[1], argv[4]);
    pthread_t thread_send, thread_recv;

    if(pthread_create(&thread_send, NULL, (void*)send_data, &sensor) != 0){
        perror("pthread_create");
        exit(EXIT_FAILURE);
    }

    if(pthread_create(&thread_recv, NULL, (void*)receive_handle, &sensor) != 0){
        perror("pthread_create");
        exit(EXIT_FAILURE);
    }

    if(pthread_join(thread_send, NULL) != 0){
        perror("pthread_join");
        exit(EXIT_FAILURE);
    }

    if(pthread_join(thread_recv, NULL) != 0){
        perror("pthread_join");
        exit(EXIT_FAILURE);
    }

    close(sockfd);
    close(recv_sockfd);
}   