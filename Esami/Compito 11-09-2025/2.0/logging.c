/*
Creare un servizio di chat in cui gli utenti possano registrarsi e accedere al servizio di chat 
e comunicare con utenti online/registrati.
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
- scriverà tutte le operazioni che vengono effettuate.
La comunicazione tra client e server principale deve essere TCP (bidirezionale), 
la connessione tra server principale e server logging è UDP (unidirezionale)
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define BUFFER_SIZE 1024
#define PORT 8081

int main(){
    int sockfd;
    char buffer[BUFFER_SIZE];
    struct sockaddr_in recv_addr, client_addr;
    socklen_t len = sizeof(client_addr);

    if((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0){
        perror("socket");
        exit(EXIT_FAILURE);
    }

    memset(&recv_addr, 0, sizeof(recv_addr));
    recv_addr.sin_family = AF_INET;
    recv_addr.sin_port = htons(PORT);
    recv_addr.sin_addr.s_addr = htonl(INADDR_ANY);

    if(bind(sockfd, (struct sockaddr*)&recv_addr, strlen(recv_addr)) < 0){
        perror("bind");
        exit(EXIT_FAILURE);
    }

    while(1){
        ssize_t n = recvfrom(sockfd, buffer, BUFFER_SIZE - 1, 0, (struct sockaddr*)&client_addr, &len);
        if(n < 0){
            perror("recvfrom");
            continue;
        }
        buffer[n] = '\0';
        printf("[LOGGING]: Operazioni effettuate: %s\n", buffer);
        fflush(stdout);
    }
    close(sockfd);
}