/*
Tema 2 PCOM
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>
#include <ctype.h>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include <poll.h>
#include <errno.h>

typedef struct {
    uint8_t command;
    uint8_t type;
    char topic[51];
    uint8_t sf;
    uint8_t ip[16];
    uint16_t port;
    char payload[1501];
} msg; //mesaj de la client la server sau de la server la client

typedef struct {
    char topic[50];
    uint8_t type;
    char payload[1500];
} msg_udp; //payload de la udp

typedef struct {
    char topic[50];
    uint8_t sf;
} sf_topic; //topicurile cu sf

void function_client(int sockfdtcp) {
    // functie asemanatoare cu cea din laboratorul 7
    char buffer[1025] = {};
    memset(buffer, 0, 1025);
    struct pollfd fds[2];
    fds[0].fd = STDIN_FILENO;
    fds[0].events = POLLIN;
    fds[1].fd = sockfdtcp;
    fds[1].events = POLLIN;
    int nfds = 2;
    int rc;
    // am setat poll 
    msg msgs;
    memset(&msgs, 0, sizeof(msg));


    while(1) {
        rc = poll(fds, nfds, -1);
        if (rc < 0) {
            perror("poll");
            exit(1);
        }

        for(int i = 0; i < nfds; i++) {
            if(fds[i].revents & POLLIN){
            if(fds[i].fd == STDIN_FILENO) {
                // citesc de la tastatura
                memset(buffer, 0, 1025);
                fgets(buffer, 1025, stdin);
                if (strncmp(buffer, "exit", 4) == 0) {
                    // daca primesc exit, inchid conexiunea
                    close(sockfdtcp);
                    exit(0);
                }
                else if(strncmp(buffer,"subscribe", 9) == 0) {
                    // daca primesc subscribe, trimit mesajul catre server
                    char *token = strtok(buffer, " ");
                    token = strtok(NULL, " ");
                    strcpy(msgs.topic, token);
                    token = strtok(NULL, " ");
                    msgs.sf = atoi(token);
                    msgs.command = 1;
                    msgs.type = 0;
                    
                    rc = send(sockfdtcp, &msgs, sizeof(msg), 0);
                    if (rc < 0) {
                        perror("send");
                        exit(1);
                    }
                    printf("Subscribed to topic.\n");
                }
                else if(strncmp(buffer,"unsubscribe", 11) == 0) {
                    // daca primesc unsubscribe, trimit mesajul catre server
                    char *token = strtok(buffer, " ");
                    token = strtok(NULL, " ");
                    strcpy(msgs.topic, token);
                    msgs.command = 2;
                    msgs.type = 0;
                    rc = send(sockfdtcp, &msgs, sizeof(msg), 0);
                    if (rc < 0) {
                        perror("send");
                        exit(1);
                    }
                    printf("Unsubscribed from topic.\n");
                }
                else {
                    printf("Comanda invalida.\n");
                }
            }
            else if(fds[i].fd == sockfdtcp) {
                // primesc mesaj de la server
                memset(&msgs, 0, sizeof(msg));
                rc = recvfrom(sockfdtcp, &msgs, sizeof(msg), 0, NULL, NULL);
                if(strncmp(msgs.payload, "exit", 4) == 0) {
                    close(sockfdtcp);
                    exit(0);
                    // exit
                }
                if (rc < 0) {
                    perror("recv");
                    exit(1);
                }
                if (rc == 0) {
                    close(sockfdtcp);
                    exit(0);
                }
                msg mesi;
                memset(&mesi, 0, sizeof(msg));
                mesi.command = msgs.command;
                mesi.type = msgs.type;
                strcpy(mesi.topic, msgs.topic);
                mesi.sf = msgs.sf;
                strcpy(mesi.payload, msgs.payload);
                // am setat mesajul primit de la server
                // afisez mesajul
                char buffer[2000];
                if (mesi.type == 0) {
                    printf("%s:%d - %s - INT - %s\n", msgs.ip, mesi.port, mesi.topic, mesi.payload);
                }
                else if (mesi.type == 1) {
                    printf("%s:%d - %s - SHORT_REAL - %s\n", msgs.ip, mesi.port, mesi.topic, mesi.payload);
                }
                else if (mesi.type == 2) {
                    printf("%s:%d - %s - FLOAT - %s\n", msgs.ip, mesi.port, mesi.topic, mesi.payload);

                }
                else if(mesi.type == 3) {
                    printf("%s:%d - %s - STRING - %s\n", msgs.ip, mesi.port, mesi.topic, mesi.payload);
                }
            }
        }
        }
    }
}

int main(int argc, char **argv) {
    // am folosit laboratorul 7 ca si schelet
    setvbuf(stdout, NULL, _IONBF, BUFSIZ);
    int sockfdtcp;
    int n;
    int rc;
    struct sockaddr_in serv_addr;
    char buffer[1500];
    
    sockfdtcp = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfdtcp < 0) {
        perror("socket");
        exit(1);
    }
    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(atoi(argv[3]));
    serv_addr.sin_addr.s_addr = inet_addr(argv[2]);
    
    rc = connect(sockfdtcp, (struct sockaddr *) &serv_addr, sizeof(serv_addr));
    if (rc < 0) {
        perror("connect");
        exit(1);
    }
    char buf[256];
    strcpy(buf, argv[1]);
    // trimit id-ul la server
    send(sockfdtcp, buf, 256, 0);
    function_client(sockfdtcp);

    close(sockfdtcp);
    return 0;
    

}