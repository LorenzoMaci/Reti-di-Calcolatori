#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <stdlib.h>


//dobbiamo creareuna socket 
int main(int argc, char* argv[]) {

  if(argc < 3){
    fprintf(stderr, "Usage: %s <IP> <Porta>\n", argv[0]);
    exit(1);    
  }

 int s, numBytes; //s = file descritto della socket, numbyte
 struct sockaddr_in sa; //sockadd_in = internetwork address 
 char buffer[BUFSIZ+1];  // max buffer for I/O operations for the system + null termination
 //conservo il messaggio in arrivo da parte del server

 if ((s = socket(PF_INET, SOCK_STREAM, 0)) < 0) { //creo una socket di tipo internetwork e socket_stream, 0 = protocollo di base
   perror("socket");
   return 1;
 }

 memset(&sa, '\0', sizeof(sa)); //la rinizializzo a zero è buona norma farlo

 sa.sin_family = AF_INET; //indico che utilizza AF_INET = ipv4, AF_INET6 = IPV6
 sa.sin_port = htons(atoi(argv[2])); //definisco la porta di ascolto in questo caso porta 13 perchè tcp funziona nella 13
 sa.sin_addr.s_addr = inet_addr(argv[1]); //indrizzo attraverso il quale vorremo conneterci

  if (connect(s, (struct sockaddr *)&sa, sizeof sa) < 0) {
      perror("connect");
      close(s);
      return 2;
  }

 while ((numBytes = read(s, buffer, BUFSIZ)) > 0)
   write(1, buffer, numBytes); // file descriptor 1 is the stdout

 close(s);
 return 0;
}

