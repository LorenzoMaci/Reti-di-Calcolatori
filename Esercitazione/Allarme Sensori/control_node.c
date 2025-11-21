/*Nodo di Controllo:
Stampa gli Allarmi
Memorizza un file di LOG di tutti gli allarmi
Dopo 5 secondi, se un Nodo Sensore entra in modalità Allarme, la interromperà*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <pthread.h>

#define BUFFER_SIZE 1024
#define RECV_CENTRAL 9091
#define PORT 9092

typedef struct{
    int id_sensore;
    char ip[INET_ADDRSTRLEN];
}allarm;

void send_allarm(void* arg){
    allarm* td = (allarm*)arg;

    sleep(5);
    int sockfd;
    struct sockaddr_in central_addr;
    char msg[BUFFER_SIZE]; //creiamo il messaggio da inviare
    snprintf(msg, sizeof(msg), "STOP_ALARM %d", td->id_sensore);

    if((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0){
        perror("socket");
        exit(EXIT_FAILURE);
    }

    memset(&central_addr, 0, sizeof(central_addr));
    central_addr.sin_family = AF_INET;
    central_addr.sin_port = htons(PORT);

    if(inet_pton(AF_INET, td->ip, &central_addr.sin_addr) <= 0){
        perror("inet_pton");
        exit(EXIT_FAILURE);
    }

    if(connect(sockfd, (struct sockaddr*)&central_addr, sizeof(central_addr)) < 0){
        perror("connect");
        exit(EXIT_FAILURE);
    }

    if(send(sockfd, msg, strlen(msg), 0) < 0){
        perror("send");
        exit(EXIT_FAILURE);
    }
       
    printf("[Nodo di Controllo] inviato comando di interruzione: %s\n", msg);
    
    close(sockfd);
    free(td);
}


int main(int argc, char* argv[]){
    if(argc != 3){
        fprintf(stderr, "Usage: %s <IP-Controllo> <IP-Centrale>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    int sockfd, client_sockfd;
    struct sockaddr_in server_addr, client_addr;
    char buffer[BUFFER_SIZE];
    socklen_t addr_len = sizeof(client_addr);
    FILE* f;
    char ip_central[INET_ADDRSTRLEN];

    strncpy(ip_central, argv[2], INET_ADDRSTRLEN);
    
    if((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0){
        perror("socket");
        exit(EXIT_FAILURE);
    }

    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(RECV_CENTRAL);

    if(inet_pton(AF_INET, argv[1], &server_addr.sin_addr) <= 0){
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

    printf("[Nodo di Controllo] in ascolto su %s:%d\n", argv[1], RECV_CENTRAL);

    if((f = fopen("allarm.log", "a")) == NULL){
        perror("fopen");
        exit(EXIT_FAILURE);
    }

    while(1){
        if((client_sockfd = accept(sockfd, (struct sockaddr*)&client_addr, &addr_len)) < 0){
            perror("accept");
            exit(EXIT_FAILURE);
        }  
        
        ssize_t n = recv(client_sockfd, buffer, BUFFER_SIZE - 1, 0);
        if(n > 0){
            buffer[n] = '\0';
            fprintf(f, "%s\n", buffer);
            printf("[Nodo di Controllo] Ricevuto: %s\n", buffer);
            fflush(f);
            
            int id_sensore = 0;
            if(sscanf(buffer, "Allarme dal sensore ID: %d", &id_sensore) == 1){    
                allarm* td = malloc(sizeof(allarm));
                td->id_sensore = id_sensore;
                strncpy(td->ip, ip_central, INET_ADDRSTRLEN);

                pthread_t thread;
                if(pthread_create(&thread, NULL, (void*)send_allarm, td) != 0){
                    perror("pthread_create");
                    exit(EXIT_FAILURE);
                }
                pthread_detach(thread);
            }
        }else{
            perror("recv");
            exit(EXIT_FAILURE);
        }

        close(client_sockfd);
    }
    fclose(f);
    close(sockfd);
}
