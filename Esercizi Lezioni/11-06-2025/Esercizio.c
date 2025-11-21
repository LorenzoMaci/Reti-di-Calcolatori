#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <protcol.h>
#include <time.h>
#include <unistd.h>


//dobbiamo definire il tipo di messasggio da inviare e dobbiamo identifcare il sensore ID
// [ID] [Timestamp]

#define MAX_BUFFER 1024
#define MSG_PROTOCOL

int socketFD;
unsigned short ID; //65K numero di cifre: ##### Identificatore
unsigned long last_timestamp; 
uint8_t cericco;
float temperature;
uint8_t humidity;
uint8_t quality;

void* gestore_ricezione(void* arg){
    char buffer[MAX_BUFFER];
    for(;;){
        srand(time(NULL));
        last_timestamp = time(NULL);
        cericco = ((rand() * 1.0 / RAND_MAX * 1.0) * 5 + 1) == 6 ? 1 : 0;
        temperature = ((rand() * 1.0 / RAND_MAX * 1.0) * 10 + 30);
        humidity = ((rand() * 1.0 / RAND_MAX * 1.0) * 100);
        quality = ((rand() * 1.0 / RAND_MAX * 1.0) * 100);
        
        sprintf(buffer, MSG_PROTOCOL, ID, last_timestamp, cericco, temperature, humidity, quality);


        send(socketFD, buffer, strlen(buffer), 0);
        printf("[MANDATO] %s \n", buffer);
        sleep(3);
    }
}

void*

int main(int argc, char* argv[]){
    if(argc < 4){
        fprintf(stderr, "Usage: %s <ID> <IP> <PORTA> \n", argv[0]);
        exit(EXIT_FAILURE);
    }

    ID = (short)atoi(argv[1]);
}