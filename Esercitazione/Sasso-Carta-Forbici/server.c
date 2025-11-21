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
#define MAX_PLAYER 10

typedef struct{
    char email[MAX_SIZE];
    char password[MAX_SIZE];
    char operation[MAX_SIZE];
    char mossa[MAX_SIZE];
    int status; //0 =off, 1 = on, 2 = wait
}player_data;

typedef struct{
    int sockfd;
    struct sockaddr_in player_addr;
    char email[MAX_SIZE];
    char password[MAX_SIZE];
    char mossa[MAX_SIZE];
    int status;   
}player_info;

typedef struct{
    player_info* players[MAX_PLAYER];
    int counter;
    pthread_mutex_t lock;
}player_list;

typedef struct{
    int sockfdp1, sockfdp2;
    player_info* p1, *p2;
}game_info;

player_list players = {.counter = 0, .lock = PTHREAD_MUTEX_INITIALIZER};

void handle_game(void* arg){
    game_info* td = (game_info*)arg;
    int sockfdp1 = td->sockfdp1, sockfdp2 = td->sockfdp2;
    player_info* p1 = td->p1, *p2 = td->p2;
    free(td);

    char buffer[BUFFER_SIZE];
    printf("Partita iniziata tra %s e %s\n", p1->email, p2->email);

    int counter1 = 0, counter2 = 0;

    for(int i = 0; i < ROUND; i++){
        printf("[ROUND-%d] in corso\n", i + 1);
        player_data data1, data2;

        if(recv(sockfdp1, &data1, sizeof(player_data), 0) < 0){
            printf("Connessione persa con %s\n", p1->email);
            pthread_exit(NULL);
        }

        if(recv(sockfdp2, &data2, sizeof(player_data), 0) < 0){
            printf("Connessione persa con %s\n", p2->email);
            pthread_exit(NULL);
        }

        char res1[BUFFER_SIZE], res2[BUFFER_SIZE];

        if(!strcmp(data1.mossa, data2.mossa)){
            snprintf(res1, sizeof(res1), "Pareggio");
            snprintf(res2, sizeof(res2), "Pareggio");
        }
        else if ((!strcmp(data1.mossa, "sasso")   && !strcmp(data2.mossa, "forbice")) ||
                (!strcmp(data1.mossa, "carta")   && !strcmp(data2.mossa, "sasso"))   ||
                (!strcmp(data1.mossa, "forbice") && !strcmp(data2.mossa, "carta"))) {
            snprintf(res1, sizeof(res1), "Hai vinto il round");
            snprintf(res2, sizeof(res2), "Hai perso il round");
            counter1++;
        } else {
            snprintf(res1, sizeof(res1), "Hai perso il round");
            snprintf(res2, sizeof(res2), "Hai vinto il round");
            counter2++;
        }

        if(send(sockfdp1, res1, strlen(res1), 0) < 0){
            perror("send");
            exit(EXIT_FAILURE);
        }

        if(send(sockfdp2, res2, strlen(res2), 0) < 0){
            perror("send");
            exit(EXIT_FAILURE);
        }        
    }

    if(counter1 > counter2){
        snprintf(buffer, sizeof(buffer), "Partita terminata: Vince %s\n", p1->email);
    }else if(counter2 > counter1){
        snprintf(buffer, sizeof(buffer), "Partita terminata: Vince %s\n", p2->email);
    }

    if(send(sockfdp1, buffer, strlen(buffer), 0) < 0){
        perror("send");
        exit(EXIT_FAILURE);
    }

    if(send(sockfdp2, buffer, strlen(buffer), 0) < 0){
        perror("send");
        exit(EXIT_FAILURE);
    }

    if(sockfdp1 >= 0) close(sockfdp1);
    if(sockfdp2 >= 0) close(sockfdp2);

    p1->status = 1;
    p2->status = 1;

    printf("Partita tra %s e %s terminata\n", p1->email, p2->email);
    pthread_exit(NULL);
}

void handle_player(void* arg){
    player_info* td = (player_info*)arg;
    int sockfd = td->sockfd;
    player_data data;
    char ip[INET_ADDRSTRLEN];
    char buffer[BUFFER_SIZE];

    inet_ntop(AF_INET, &td->player_addr.sin_addr, ip, INET_ADDRSTRLEN);
    int port = ntohs(td->player_addr.sin_port);

    printf("[SERVER]: Nuova connessione da %s:%d\n", ip, port);

    while(1){
        ssize_t n = recv(sockfd, &data, sizeof(player_data), 0);
        if(n <= 0){
            if(n == 0){
                printf("Chiusura connessione\n");
            }
            perror("recv");
            break;
        }

        if(!strcmp(data.operation, "registrazione")){
            printf("[SERVER] Richiesta registrazione da %s\n", data.email);
            
            pthread_mutex_lock(&players.lock);
            int found = 0;
            for(int i = 0; i < players.counter; i++){
                if(!strcmp(players.players[i]->email, data.email) && !strcmp(players.players[i]->password, data.password)){
                    found = 1;
                    break;
                }
            }

            if(!found && players.counter < MAX_PLAYER){
                player_info* new_player = malloc(sizeof(player_info));
                strcpy(new_player->email, data.email);
                strcpy(new_player->password, data.password);
                new_player->status = 0;
                new_player->sockfd = sockfd;
                players.players[players.counter++] = new_player;
                printf("[SERVER] Nuovo player '%s' registrato\n", data.email);
                snprintf(buffer, sizeof(buffer), "REG_OK");
            }else{
                printf("[SERVER] Utente %s già registrato\n", data.email);
                snprintf(buffer, sizeof(buffer), "Utente già registrato");
            }

            pthread_mutex_unlock(&players.lock);

            if(send(sockfd, buffer, strlen(buffer), 0) < 0){
                perror("send");
                exit(EXIT_FAILURE);
            } 
        }
        else if(!strcmp(data.operation, "login")){
            printf("[SERVER] Richiesta di login da parte di %s\n", data.email);

            pthread_mutex_lock(&players.lock);
            int found = 0;
            for(int i = 0; i < players.counter; i++){
                if(!strcmp(players.players[i]->email, data.email) && !strcmp(players.players[i]->password, data.password)){
                    found = 1;
                    players.players[i]->status = 1;
                    players.players[i]->sockfd = sockfd;
                    td = players.players[i];
                    printf("[SERVER] login avvenuto con successo per %s\n", data.email);
                    break;
                }                
            }

            pthread_mutex_unlock(&players.lock);

            if(found){
                printf("[SERVER] utente %s trovato ora è online\n", data.email);
                snprintf(buffer, sizeof(buffer), "LOG_OK");
            }else{
                printf("[SERVER] utente non trovato o email o passowrd errate\n");
                snprintf(buffer, sizeof(buffer), "Email o Passowrd errate");
            }

            if(send(sockfd, buffer, strlen(buffer), 0) < 0){
                perror("send");
                exit(EXIT_FAILURE);
            }
        }else if(!strcmp(data.operation, "match")){
            printf("[SERVER] Richiesta di matchmaking da %s\n", data.email);

            pthread_mutex_lock(&players.lock);
            player_info* found = NULL;
            td->status = 2;

            for(int i = 0; i < players.counter; i++){
                if(players.players[i]->status == 2 && players.players[i] != td){
                    found = players.players[i];
                    break;
                }
            }

            pthread_mutex_unlock(&players.lock);

            if(found){
                pthread_mutex_lock(&players.lock);
                td->status = 3;
                found->status = 3;
                pthread_mutex_unlock(&players.lock);
            
                int sockfd_game; //Creo la socket del gioco
                if((sockfd_game = socket(AF_INET, SOCK_STREAM, 0)) < 0){
                    perror("socket");
                    exit(EXIT_FAILURE);
                }

                struct sockaddr_in game_addr; 
                memset(&game_addr, 0, sizeof(game_addr));
                game_addr.sin_family = AF_INET;
                game_addr.sin_port = htons(0); //porta effimera 
                game_addr.sin_addr.s_addr = htonl(INADDR_ANY);

                if(bind(sockfd_game, (struct sockaddr*)&game_addr, sizeof(game_addr)) < 0){
                    perror("bind");
                    exit(EXIT_FAILURE);
                }

                if(listen(sockfd_game, 2) < 0){
                    perror("listen");
                    exit(EXIT_FAILURE);
                }

                socklen_t len = sizeof(game_addr); //recupero la porta scelta dal kernel
                if(getsockname(sockfd_game, (struct sockaddr*)&game_addr, &len) < 0){
                    perror("getsockname");
                    exit(EXIT_FAILURE);
                }

                int game_port = ntohs(game_addr.sin_port); //ottengo la porta

                snprintf(buffer, sizeof(buffer), "MATCH_FOUND %d", game_port); //invio il messaggio ai player con la porta
                //invio il messaggio ai due player che devono giocare
                if(send(sockfd, buffer, strlen(buffer), 0) < 0){ 
                    perror("send");
                    exit(EXIT_FAILURE);
                }

                if(send(found->sockfd, buffer, strlen(buffer), 0) < 0){
                    perror("send");
                    exit(EXIT_FAILURE);
                }

                int sockfdp1, sockfdp2;
                if((sockfdp1 = accept(sockfd_game, NULL, NULL)) < 0){
                    perror("accept");
                    continue;
                }

                if((sockfdp2 = accept(sockfd_game, NULL, NULL)) < 0){
                    perror("accept");
                    continue;
                }

                close(sockfd_game);

                if(sockfdp1 < 0 || sockfdp2 < 0){
                    if(sockfdp1 >= 0) close(sockfdp1);
                    if(sockfdp2 >= 0) close(sockfdp2);
                    pthread_mutex_lock(&players.lock);
                    td->status = 1;
                    found->status = 1;
                    pthread_mutex_unlock(&players.lock);
                    return;
                }

                game_info* games = malloc(sizeof(game_info));
                games->p1 = td;
                games->p2 = found;
                games->sockfdp1 = sockfdp1;
                games->sockfdp2 = sockfdp2;

                pthread_t game_thread;
                if(pthread_create(&game_thread, NULL, (void*)handle_game, games) != 0){
                    perror("pthread_create");
                    exit(EXIT_FAILURE);
                }

                pthread_detach(game_thread);
            }else{
                snprintf(buffer, sizeof(buffer), "WAIT");
                if(send(sockfd, buffer, strlen(buffer), 0) < 0){
                    perror("send");
                    exit(EXIT_FAILURE);
                }
            }
        }else if(!strcmp(data.operation, "wait")){
            printf("[SERVER] utente %s in stato di attesa\n", data.email);
            pthread_mutex_lock(&players.lock);
            td->status = 2;
            pthread_mutex_unlock(&players.lock);
            snprintf(buffer, sizeof(buffer), "WAIT");
            if(send(sockfd, buffer, strlen(buffer), 0) < 0){
                perror("send");
                exit(EXIT_FAILURE);
            }
        }else if(!strcmp(data.operation, "exit")){
            printf("Chiusura connessione\n");
            break;
        }       
    }
    close(sockfd);
    pthread_exit(NULL);
}

int main(){
    int sockfd;
    struct sockaddr_in server_addr;

    if((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0){
        perror("socket");
        exit(EXIT_FAILURE);
    }

    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(PORT);
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);

    if(bind(sockfd, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0){
        perror("bind");
        exit(EXIT_FAILURE);
    }

    if(listen(sockfd, MAX_PLAYER) < 0){
        perror("listen");
        exit(EXIT_FAILURE);
    }

    printf("[SERVER] in ascolto su %d\n", PORT);

    while(1){
        player_info* player = malloc(sizeof(player_info));
        socklen_t len = sizeof(player->player_addr);

        if((player->sockfd = accept(sockfd, (struct sockaddr*)&player->player_addr, &len)) < 0){
            perror("accept");
            exit(EXIT_FAILURE);
        }

        pthread_t tid;
        if(pthread_create(&tid, NULL, (void*)handle_player, player)  != 0){
            perror("pthread_create");
            close(player->sockfd);
            free(player);
        }

        pthread_detach(tid);
    }

    pthread_mutex_destroy(&players.lock);
    close(sockfd);
}