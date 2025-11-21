/*Scrivere un client TCP interattivo che:
si connetta a un server specificato da indirizzo IP e porta;
legga righe da tastiera e le invii al server;
riceva la risposta del server e la mostri all’utente;
gestisca il caso in cui il server non risponde entro 5 secondi, stampando un messaggio di timeout;
chiuda la connessione quando il server invia "BYE" o in caso di errori. 
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <unistd.h>
#include <sys/select.h>
#include <sys/types.h>

#define MAX_LENGTH 1024

int main(int argc, char* argv[]){
    if(argc != 3){
        fprintf(stderr, "Usage: %s <ip_server> <port>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    int sockfd;
    struct sockaddr_in server_addr;

    if((sockfd = socket(AF_INET, SOCK_STREAM, 0))< 0){
        perror("socket");
        exit(EXIT_FAILURE);
    }

    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(atoi(argv[2]));

    if(inet_pton(AF_INET, argv[1], &server_addr.sin_addr) <= 0){
        perror("inet_pton");
        exit(EXIT_FAILURE);
    }
    
    if(connect(sockfd, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0){
        perror("connect");
        exit(EXIT_FAILURE);
    }

    char line[MAX_LENGTH];
    printf("Inserisci operazioni: ");
    while(fgets(line, sizeof(line), stdin)){
        if(line[0] == '\n' || line[0] == '\0'){
            continue;
        }
        if(line[strlen(line) - 1] != '\n'){
            if(strlen(line) < MAX_LENGTH - 1){ //evito buffer overflow
                line[strlen(line)] = '\n';
                line[strlen(line) - 1] = '\0';
             }
        }

        if(send(sockfd, line, strlen(line), 0) < 0){
            perror("send");
            exit(EXIT_FAILURE);
        }

        fd_set fds;
        FD_ZERO(&fds);
        FD_SET(sockfd, &fds);
        struct timeval tv = {.tv_sec = 5, .tv_usec = 0};
        int rv = select(sockfd + 1, &fds, NULL, NULL, &tv);   

        if(rv == 0){
            printf("Timeout Server\n");
            break;
        }else if(rv < 0){
            perror("select");
            break;
        }

        char buffer[MAX_LENGTH];
        ssize_t n = recv(sockfd, buffer, sizeof(buffer) - 1, 0);
        if(n <= 0){
            perror("recv");
            break;
        }
        buffer[n] = '\0';

        printf("> %s", buffer);

        if(!strncmp(buffer, "BYE\n", 4)){
            printf(" Client Quit\n");
            exit(EXIT_SUCCESS);
        }
    }
    close(sockfd);
}
