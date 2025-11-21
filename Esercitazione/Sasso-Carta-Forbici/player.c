/*Crea una partita in rete di sasso, carta e forbici.
Giocatore:
Il giocatore può connettersi a un server
Il giocatore deve registrarsi utilizzando e-mail e password
Il giocatore deve effettuare l'accesso per poter eseguire azioni
Un giocatore può cercarne un altro con cui giocare
Poi un giocatore può rimanere in stato di ATTESA (questa è una scelta del giocatore) 
Quando il server trova un avversario, la partita inizia
Una partita è composta da 3 round

Server di gioco:
Il server di gioco gestisce la registrazione e l'autenticazione dei giocatori.
Il server di gioco deve gestire il matchmaking per creare partite per due giocatori
Il matchmaking può essere progettato prendendo i primi due giocatori in attesa o casualmente tra i giocatori in attesa
Il server è l'autorità per controllare lo stato della partita e le mosse dei giocatori
Al termine di una partita, i giocatori tornano nella lobby (non in stato di attesa)
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
#define MAX_SIZE 100
#define PORT 6060
#define ROUND 3

typedef struct{
    char email[MAX_SIZE];
    char password[MAX_SIZE];
    char operation[MAX_SIZE];
    char mossa[MAX_SIZE];
    int status; //0 =off, 1 = on, 2 = wait
}player_data;

int sockfd;
int sockfd_game = -1;
char ip_server[INET_ADDRSTRLEN];

void start_game(void* arg){
    player_data* td = (player_data*)arg;
    char buffer[BUFFER_SIZE];

    if(sockfd_game < 0){
        fprintf(stderr, "socket del gioco non inizializzata\n");
        pthread_exit(NULL);
    }

    for(int i = 0; i < ROUND; i++){
        printf("[ROUND-%d] insericsi mossa (sasso, carta, forbice): ", i + 1);
        fgets(td->mossa, MAX_SIZE, stdin);
        td->mossa[strcspn(td->mossa, "\n")] = '\0';

        strcpy(td->operation, "mossa");

        if(send(sockfd_game, td, sizeof(player_data), 0) < 0){
            perror("send");
            exit(EXIT_FAILURE);
        }

        ssize_t n = recv(sockfd_game, buffer, BUFFER_SIZE -1 , 0);
        if(n <= 0){
            if(n == 0){
                printf("Chiusura connessione\n");
            }
            perror("recv");
            break;
        }

        buffer[n] = '\0';

        printf("\n[ROUND-%d]: %s\n", i + 1, buffer);
        fflush(stdout);
    }

    ssize_t n = recv(sockfd_game, buffer, BUFFER_SIZE - 1,0);
    if(n > 0){
        buffer[n] = '\0';
        printf("\n[ESITO FINALE]: %s\n", buffer);
        fflush(stdout);
    }

    printf("Partita terminata ritorno online\n");
    td->status = 1;

    if(sockfd_game >= 0){
        close(sockfd_game);
        sockfd_game = -1;
    }
    pthread_exit(NULL);
}

void receive_handle(void* arg){
    player_data* td = (player_data*)arg;
    char buffer[BUFFER_SIZE];

    while(1){
        ssize_t n = recv(sockfd, buffer, BUFFER_SIZE - 1, 0);
        if(n <= 0){
            if(n == 0){
                printf("Chiusura connessione\n");
            }
            perror("recv");
            exit(EXIT_FAILURE);
        }

        buffer[n] = '\0';

        if(!strcmp(buffer, "REG_OK")){
            printf("\nRegistrazione avvenuta con successo\n");
            fflush(stdout);
        }
        else if(!strcmp(buffer, "LOG_OK")){
            td->status = 1;
            printf("\nLogin avvenuto con successo\n");
            fflush(stdout);
        }
        else if(!strncmp(buffer, "MATCH_FOUND", 11)){
            int port = 0;

            if(sscanf(buffer, "MATCH_FOUND %d", &port) == 1){
                struct sockaddr_in game_addr;

                if((sockfd_game = socket(AF_INET, SOCK_STREAM, 0)) < 0){
                    perror("socket");
                    exit(EXIT_FAILURE);
                }

                memset(&game_addr, 0, sizeof(game_addr));
                game_addr.sin_family = AF_INET;
                game_addr.sin_port = htons(port);

                if(inet_pton(AF_INET, ip_server, &game_addr.sin_addr) <= 0){
                    perror("inet_pton");
                    exit(EXIT_FAILURE);
                }

                if(connect(sockfd_game, (struct sockaddr*)&game_addr, sizeof(game_addr)) < 0){
                    perror("connect");
                    close(sockfd_game);
                    sockfd_game = -1;
                    continue;
                }
                printf("\nAvversario trovato inizia la partita\n");

                pthread_t game;
                if(pthread_create(&game, NULL, (void*)start_game, td) != 0){
                    perror("pthread_create");
                    exit(EXIT_FAILURE);
                }

                pthread_detach(game);
            }   
        }else if(!strcmp(buffer, "WAIT")){
            printf("\nAttendo...\n");
            fflush(stdout);
        }else{
            printf("\nRiceuvto: %s\n", buffer);
            fflush(stdout);
        }
    }

}

void handle_data(void* arg){
    player_data* td = (player_data*)arg;
    char buffer[BUFFER_SIZE];

    while(1){
        if(td->status == 0){
            printf("Operazioni disponibili, registrazione, login\n");
            printf("Inserisci Operazione: ");

            if(!fgets(buffer, BUFFER_SIZE, stdin)) continue;
            buffer[strcspn(buffer, "\n")] = '\0';

            if(!strcmp(buffer, "registrazione") || !strcmp(buffer, "login")){
                strcpy(td->operation, buffer);
                printf("Email: ");
                fgets(td->email, MAX_SIZE, stdin);
                td->email[strcspn(td->email, "\n")] = '\0';

                printf("Password: ");
                fgets(td->password, MAX_SIZE, stdin);
                td->password[strcspn(td->password, "\n")] = '\0';

                if(send(sockfd, td, sizeof(player_data), 0) < 0){
                    perror("send");
                    exit(EXIT_FAILURE);
                }
            }
        }
        else if(td->status == 1){
            printf("Operazioni disponibili, match, wait, exit\n");
            printf("Inserisci operazione: ");
            
            if(!fgets(buffer, BUFFER_SIZE, stdin)) continue;
            buffer[strcspn(buffer, "\n")] = '\0';

            if(!strcmp(buffer, "match") || !strcmp(buffer, "wait")){
                if(!strcmp(buffer, "match")){
                    printf("Richiesta di matchmaking in corso...\n");
                }else{
                    printf("In fase di attesa\n");
                }

                strcpy(td->operation, buffer);
                td->status = 2;

                if(send(sockfd, td, sizeof(player_data), 0) < 0){
                    perror("send");
                    exit(EXIT_FAILURE);
                }
            }else if(!strcmp(buffer, "exit")){
                strcpy(td->operation, buffer);
                printf("Chiusura connessione\n");
                if(send(sockfd, td, sizeof(player_data), 0) < 0){
                    perror("send");
                    exit(EXIT_FAILURE);
                }
                close(sockfd);
                exit(EXIT_SUCCESS);
            }
        }
    }   
}

int main(int argc, char* argv[]){
    if(argc != 2){
        fprintf(stderr, "Usage: %s <ip_server>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    player_data td;
    td.status = 0;
    strcpy(ip_server, argv[1]);
    
    if((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0){
        perror("socket");
        exit(EXIT_FAILURE);
    }

    struct sockaddr_in server_addr;

    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(PORT);

    if(inet_pton(AF_INET, argv[1], &server_addr.sin_addr) <= 0){
        perror("inet_pton");
        exit(EXIT_FAILURE);
    }

    if(connect(sockfd, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0){
        perror("connect");
        exit(EXIT_FAILURE);
    }

    printf("Player connesso a %s:%d\n", argv[1], PORT);

    pthread_t recv_thread, send_thread;

    if(pthread_create(&recv_thread, NULL, (void*)receive_handle, &td) != 0){
        perror("pthread_create");
        exit(EXIT_FAILURE);
    }

    if(pthread_create(&send_thread, NULL, (void*)handle_data, &td) != 0){
        perror("pthread_create");
        exit(EXIT_FAILURE);
    }

    pthread_join(recv_thread, NULL);
    pthread_join(send_thread, NULL);

    close(sockfd);
}