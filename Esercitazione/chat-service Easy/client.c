/* Create a Chat Service where users can register and login into the chat service and communicate to specific users.
Users:
Can register using email and password
Can send a message to specific user (if connected)


Main Server:
Manages the registration and authentication 
Manage the communication system between users connected
The users information must be saved into a database (a file)
When startup, the service must reload the database (the file)
*/
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <pthread.h>

#define BUFFER_SIZE 1024
#define USER_SIZE 100

typedef enum{REGISTER = 'r', LOGIN = 'l', SEND = 's'} Operation;

typedef struct{
    Operation op;
    char user_account[USER_SIZE];
    char user_password[USER_SIZE];
    int connect; 
    char receiver_name[USER_SIZE];
    char ms[BUFFER_SIZE];
    char ip[INET_ADDRSTRLEN];
} Message;

int sockfd;

void receive_handler(void* arg){
    char buffer[BUFFER_SIZE];
    int numBytes;

    while((numBytes = recv(sockfd, buffer, BUFFER_SIZE - 1, 0)) > 0){
        buffer[numBytes] = '\0';
        printf("\n📨 Messaggio ricevuto: %s\n> ", buffer);
        fflush(stdout);
    }

    printf("\nConnessione chiusa dal server\n");
    exit(EXIT_SUCCESS);
}

void send_handler(void* arg){
    char buffer[BUFFER_SIZE];
    Message msg;
    char op;

    while(1){
        printf("Operazione (r = REGISTER, l = LOGIN, s = SEND): ");
        op = getchar();
        while(getchar() != '\n'){} // pulisce il buffer

        memset(&msg, 0, sizeof(Message));
        msg.op = op;

        switch(op){
            case REGISTER:
                printf("Username: ");
                fgets(msg.user_account, USER_SIZE, stdin);
                msg.user_account[strcspn(msg.user_account, "\n")] = '\0';

                printf("Password: ");
                fgets(msg.user_password, USER_SIZE, stdin);
                msg.user_password[strcspn(msg.user_password, "\n")] = '\0';
                break;

            case LOGIN:
                printf("Username: ");
                fgets(msg.user_account, USER_SIZE, stdin);
                msg.user_account[strcspn(msg.user_account, "\n")] = '\0';

                printf("Password: ");
                fgets(msg.user_password, USER_SIZE, stdin);
                msg.user_password[strcspn(msg.user_password, "\n")] = '\0';
                break;

            case SEND:
                printf("User a cui inviare il messaggio: ");
                fgets(msg.receiver_name, USER_SIZE, stdin);
                msg.receiver_name[strcspn(msg.receiver_name, "\n")] = '\0';

                printf("Messaggio: ");
                fgets(msg.ms, BUFFER_SIZE, stdin);
                msg.ms[strcspn(msg.ms, "\n")] = '\0';
                break;

            default:
                printf("Operazione non valida\n");
                continue;
        }

        if(send(sockfd, &msg, sizeof(msg), 0) == -1){
            perror("send");
            exit(EXIT_FAILURE);
        }
    }
}

int main(int argc, char* argv[]){
    if(argc != 3){
        fprintf(stderr, "Usage: %s <Indirizzo IPv4> <Port>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    struct sockaddr_in server_addr;

    if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
        perror("socket");
        exit(EXIT_FAILURE);
    }

    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(atoi(argv[2]));

    if(inet_pton(AF_INET, argv[1], &server_addr.sin_addr) <= 0){
        perror("invalid address");
        exit(EXIT_FAILURE);
    }

    if(connect(sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("connection failed");
        exit(EXIT_FAILURE);
    }

    printf("Connected a %s:%s\n", argv[1], argv[2]);
    printf("Comandi:\n - r = REGISTER\n - l = LOGIN\n - s = SEND\n");

    pthread_t receive_thread, send_thread;
    if(pthread_create(&receive_thread, NULL, (void*)receive_handler, NULL) != 0){
        perror("pthread_create receive");
        exit(EXIT_FAILURE);
    }

    if(pthread_create(&send_thread, NULL, (void*)send_handler, NULL) != 0){
        perror("pthread_create send");
        exit(EXIT_FAILURE);
    }

    if(pthread_join(receive_thread, NULL) != 0){
        perror("pthread_join receive");
        exit(EXIT_FAILURE);
    }

    if(pthread_join(send_thread, NULL) != 0){
        perror("pthread_join send");
        exit(EXIT_FAILURE);
    }

    close(sockfd);
}
