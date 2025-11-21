/*
Creare un servizio di chat in cui gli utenti possano registrarsi e accedere al servizio di chat 
e comunicare con utenti online.
Client: 
- Possono registrarsi utilizzando e-mail e password 
- quando gli utenti sono online Possono inviare un messaggio a un utente specifico (se connesso) 
- La comunicazione tra client e server deve essere TCP
- Nel caso in cui l'utente non è connesso il client potrà inviare i messaggi e il server invierà i messaggi non appena l'utente si connetterà 
Server principale: 
- Gestire la registrazione e l'autenticazione (NON TRAMITE FILE IL SERVER NON APPENA SI CHIUDE NON RICORDA PIU' NULLA / Quando il client si disconette)
- Gestire l'invio corretto del messaggio tra client A e client B
- Nel caso in cui l'utente sia offline salvare 10 messaggi dal più recente al meno recente e appena l'utente si connetterà verranno inviati 
- dovrà inviare le operazioni effettuate al server logging, in una comunicazione UDP conoscendo solo la porta, il server principale non è a conoscenza dell'indirizzo IP 
Server logging: 
- scriverà tutte le operazioni che vengono effettuate sul terminale.
La comunicazione tra client e server principale deve essere TCP (bidirezionale), 
la connessione tra server principale e server logging è UDP (unidirezionale)
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <pthread.h>

#define BUFFER_SIZE 1024
#define PORT 7777
#define MAX_SIZE 100


int main(){
    int sockfd;
    struct sockaddr_in log_addr;

    if((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0){
        perror("socket");
        exit(EXIT_FAILURE);
    }

    memset(&log_addr, 0, sizeof(log_addr));
    log_addr.sin_family = AF_INET;
    log_addr.sin_port = htons(PORT);
    log_addr.sin_addr.s_addr = htonl(INADDR_ANY);

    if(bind(sockfd, (struct sockaddr*)&log_addr, sizeof(log_addr)) < 0){
        perror("bind");
        exit(EXIT_FAILURE);
    }

    printf("[LOG] in ascolto su %d\n", PORT);

    FILE* f;

    if((f = fopen("file.txt", "a")) == NULL){
        perror("fopen");
        exit(EXIT_FAILURE);
    }

    char buffer[BUFFER_SIZE];
    struct sockaddr_in from_addr;
    socklen_t len = sizeof(from_addr);
    while(1){
        ssize_t n = recvfrom(sockfd, buffer, BUFFER_SIZE -1, 0, (struct sockaddr*)&from_addr, &len);

        if(n > 0){
            buffer[n] = '\0';
            fprintf(f, "%s\n", buffer);
            fflush(f);
            printf("[LOG] Ricevuta operazione: %s\n",buffer);
            fflush(stdout);

            if(!strcmp(buffer, "exit")){
                printf("Chiusura connessione\n");
                break;
            }
        }
    }
    close(sockfd);
    return 0;
}