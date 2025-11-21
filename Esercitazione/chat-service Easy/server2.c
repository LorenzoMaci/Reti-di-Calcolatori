#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define PORT 8080
#define MAX_CLIENTS 10
#define BUFFER_SIZE 1024
#define USER_SIZE 100

typedef enum { REGISTER = 'r', LOGIN = 'l', SEND = 's' } Operation;

typedef struct {
    Operation op;
    char user_account[USER_SIZE];
    char user_password[USER_SIZE];
    int connect;
    char receiver_name[USER_SIZE];
    char ms[BUFFER_SIZE];
    char ip[INET_ADDRSTRLEN];
} Message;

typedef struct {
    int sockfd;
    struct sockaddr_in address;
    char user_account[USER_SIZE];
} Client;

Client connected_clients[MAX_CLIENTS];
int connected_count = 0;
pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;

int Login(Message* msg, FILE* f) {
    rewind(f);
    char buffer[BUFFER_SIZE];
    char* username, *password, *ip;
    while (fgets(buffer, BUFFER_SIZE, f)) {
        username = strtok(buffer, " ");
        password = strtok(NULL, " ");
        ip = strtok(NULL, " \n");
        if (username && password && ip &&
            strcmp(username, msg->user_account) == 0 &&
            strcmp(password, msg->user_password) == 0 &&
            strcmp(ip, msg->ip) == 0) {
            return 1;
        }
    }
    return 0;
}

int Register(Message* msg, FILE* f) {
    if (Login(msg, f)) return 0;
    fseek(f, 0, SEEK_END);
    fprintf(f, "%s %s %s\n", msg->user_account, msg->user_password, msg->ip);
    fflush(f);
    return 1;
}

void handle_client(void* arg) {
    Client* td = (Client*)arg;
    int sockfd = td->sockfd;
    char buffer[BUFFER_SIZE];
    char client_ip[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &td->address.sin_addr, client_ip, INET_ADDRSTRLEN);
    int client_port = ntohs(td->address.sin_port);
    printf("New connection from %s:%d\n", client_ip, client_port);

    FILE* db;
    Message msg;
    while (1) {
        memset(&msg, 0, sizeof(msg));
        if (recv(sockfd, &msg, sizeof(msg), 0) <= 0) {
            close(sockfd);
            pthread_exit(NULL);
        }

        strcpy(msg.ip, client_ip);

        db = fopen("database.txt", "a+");
        if (!db) {
            perror("Errore apertura file");
            close(sockfd);
            pthread_exit(NULL);
        }

        if (msg.op == REGISTER) {
            if (Register(&msg, db))
                send(sockfd, "REG_OK", strlen("REG_OK"), 0);
            else
                send(sockfd, "REG_FAIL", strlen("REG_FAIL"), 0);
        } 
        else if (msg.op == LOGIN) {
            if (Login(&msg, db)) {
                pthread_mutex_lock(&lock);
                if (connected_count < MAX_CLIENTS) {
                    strcpy(connected_clients[connected_count].user_account, msg.user_account);
                    connected_clients[connected_count].sockfd = sockfd;
                    connected_count++;
                }
                pthread_mutex_unlock(&lock);
                send(sockfd, "LOGIN_OK", strlen("LOGIN_OK"), 0);
            } else {
                send(sockfd, "LOGIN_FAIL", strlen("LOGIN_FAIL"), 0);
            }
        } 
        else if (msg.op == SEND) {
            int found = 0;
            pthread_mutex_lock(&lock);
            for (int i = 0; i < connected_count; i++) {
                if (strcmp(connected_clients[i].user_account, msg.receiver_name) == 0) {
                    snprintf(buffer, BUFFER_SIZE, "[%s] %s", msg.user_account, msg.ms);
                    send(connected_clients[i].sockfd, buffer, strlen(buffer), 0);
                    found = 1;
                    break;
                }
            }
            pthread_mutex_unlock(&lock);

            if (found)
                send(sockfd, "MESSAGE_OK", strlen("MESSAGE_OK"), 0);
            else
                send(sockfd, "USER_NOT_CONNECTED", strlen("USER_NOT_CONNECTED"), 0);
        }

        fclose(db);
    }

    close(sockfd);
    pthread_exit(NULL);
}

int main() {
    int server_fd, new_socket;
    struct sockaddr_in address;
    socklen_t addrlen = sizeof(address);

    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        perror("socket failed");
        exit(EXIT_FAILURE);
    }

    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(PORT);

    if (bind(server_fd, (struct sockaddr*)&address, sizeof(address)) < 0) {
        perror("bind failed");
        exit(EXIT_FAILURE);
    }

    if (listen(server_fd, 5) < 0) {
        perror("listen");
        exit(EXIT_FAILURE);
    }

    printf("✅ Server in ascolto sulla porta %d...\n", PORT);

    while (1) {
        new_socket = accept(server_fd, (struct sockaddr*)&address, &addrlen);
        if (new_socket < 0) {
            perror("accept");
            continue;
        }

        pthread_t tid;
        int* pclient = malloc(sizeof(int));
        *pclient = new_socket;
        pthread_create(&tid, NULL, (void*)handle_client, pclient);
        pthread_detach(tid);
    }

    return 0;
}
