/*Sviluppare una soluzione software che consenta a un Nodo Sensore di inviare alcuni dati dei sensori a uno specifico Nodo Centrale.
Quando un Nodo Centrale riceve dati dal sensore, se si verifica una condizione specifica, il Nodo Centrale invierà un allarme a un Nodo di Controllo.

Nodo Sensore:
Può inviare dati ogni 3 secondi
La comunicazione potrebbe non essere affidabile
I dati del sensore potrebbero essere temperatura, umidità e qualità dell'aria
Ogni sensore deve essere identificato da un ID univoco

Nodo Centrale:
Memorizzerà un nuovo sensore quando riceve un messaggio da esso
Se la temperatura è superiore a 30°C o la qualità dell'aria è scarsa, deve inviare un messaggio al Nodo di Controllo
La comunicazione deve essere affidabile

Nodo di Controllo:
Stamperà gli allarmi
Memorizzerà un file di LOG di tutti gli allarmi*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>

#define PORT 9100
#define BUFFER_SIZE 1024

int main() {
    int server_fd, client_fd;
    struct sockaddr_in server_addr, client_addr;
    socklen_t addr_len = sizeof(client_addr);
    char buffer[BUFFER_SIZE];
    FILE* f;

    // Crea socket TCP
    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("socket");
        exit(EXIT_FAILURE);
    }

    // Imposta indirizzo e porta
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(PORT);

    // Bind
    if (bind(server_fd, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        perror("bind");
        exit(EXIT_FAILURE);
    }

    // Listen
    if (listen(server_fd, 5) < 0) {
        perror("listen");
        exit(EXIT_FAILURE);
    }

    printf("Nodo di Controllo in ascolto sulla porta %d...\n", PORT);

    // Apri file di log
    if((f = fopen("file.log", "a")) == NULL) {
        perror("fopen");
        exit(EXIT_FAILURE);
    }

    while (1) {
        client_fd = accept(server_fd, (struct sockaddr*)&client_addr, &addr_len);
        if (client_fd < 0) {
            perror("accept");
            continue;
        }

        int n = recv(client_fd, buffer, BUFFER_SIZE - 1, 0);
        if (n > 0) {
            buffer[n] = '\0';
            printf("ALLARME RICEVUTO: %s\n", buffer);
            fprintf(f, "%s\n", buffer);
            fflush(f);
        }
        close(client_fd);
    }

    fclose(f);
    close(server_fd);
}