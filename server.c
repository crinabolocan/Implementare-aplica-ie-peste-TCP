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
#include <string.h>
#include "server.h"

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
    char topic[51];
    uint8_t type;
    char payload[1501];
} msg_udp; //payload de la udp

typedef struct {
    char topic[50];
    uint8_t sf;
} sf_topic; //topicurile cu sf

typedef struct {
    char id[10];
    int connected;
    int sockfd;
    char topics[100][50];
    int sf[100];
    int nr_topics;
} client; //structura pentru clienti


int main(int argc, char **argv) {
    setvbuf(stdout, NULL, _IONBF, BUFSIZ);

    // un socket tcp 
    int sockfdtcp;
    sockfdtcp = socket(AF_INET, SOCK_STREAM, 0);
    if(sockfdtcp < 0) {
        perror("socket");
        exit(-1);
    }
    // un socket udp
    int sockfdudp;
    sockfdudp = socket(AF_INET, SOCK_DGRAM, 0);
    if(sockfdudp < 0) {
        perror("socket");
        exit(-1);
    }
    // facem socketul udp sa poata fi refolosit
    if(setsockopt(sockfdudp, SOL_SOCKET, SO_REUSEADDR, &(int){1}, sizeof(int)) < 0) {
        perror("setsockopt");
        exit(-1);
    }

    int port = atoi(argv[1]);
    int ret;
    struct sockaddr_in server_addr, client_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);
    server_addr.sin_addr.s_addr = INADDR_ANY;

    memset(&client_addr, 0, sizeof(client_addr));
    client_addr.sin_family = AF_INET;
    client_addr.sin_port = htons(port);
    client_addr.sin_addr.s_addr = INADDR_ANY;
    // bind la socketi, asociem socketii la adresa serverului
    ret = bind(sockfdtcp, (struct sockaddr *) &server_addr, sizeof(server_addr));
    if(ret < 0) {
        perror("bind");
        exit(-1);
    }
    // bind la socketi, asociem socketii la adresa serverului
    ret = bind(sockfdudp, (struct sockaddr *) &server_addr, sizeof(server_addr));
    if(ret < 0) {
        perror("bind");
        exit(-1);
    }
    // listen pe socketul tcp
    ret = listen(sockfdtcp, 100);
    if(ret < 0) {
        perror("listen");
        exit(-1);
    }
    
    // strcutura de poll
    struct pollfd fds[100];
    int nfds = 1;
    //initializam structura de poll
    fds[0].fd = sockfdtcp;
    fds[0].events = POLLIN;
    fds[1].fd = sockfdudp;
    fds[1].events = POLLIN;
    fds[2].fd = STDIN_FILENO;
    fds[2].events = POLLIN;
    nfds = 3;

    char buffer[2000];
    int flag = 1;
    char buffer2[2000];
    client *c = NULL;
    int nr_clienti = 0;

    while(1) {
        //facem poll 
        ret = poll(fds, nfds, -1);
        if(ret < 0) {
            perror("poll");
            exit(-1);
        }

        for(int i = 0; i < nfds; i++) {
            if(fds[i].revents & POLLIN) {
                if(fds[i].fd == STDIN_FILENO) {
                    //citim de la tastatura
                    memset(buffer, 0, 2000);
                    fgets(buffer, 2000, stdin);
                    // daca am primit exit, iesim din program
                    if(strncmp(buffer, "exit", 4) == 0) {
                        flag = 0;
                        break;
                    }
                }
                else if(fds[i].fd == sockfdtcp) {
                    // daca am primit conexiune noua, client nou
                    int newsockfd;
                    socklen_t client_len = sizeof(client_addr);
                    // acceptam conexiunea
                    newsockfd = accept(sockfdtcp, (struct sockaddr *) &client_addr, &client_len);
                    if(newsockfd < 0) {
                        perror("accept");
                        exit(-1);
                    }
                    // se primesc date de la client, si le receptionam
                    int n = recv(newsockfd, buffer, 2000, 0);
                    if(n < 0) {
                        perror("recv");
                        exit(-1);
                    }
                    //imi modific bufferul mai incolo si l am salvat ca sa nu pierd clientul
                    memset(buffer2, 0, 2000);
                    strcpy(buffer2, buffer);
                    int connected = 0;
                    int s = 0;
                    for(int j = 0; j < nr_clienti; j++) {
                        // verific daca clientul e in lista de clienti
                        if(strncmp(buffer, c[j].id, strlen(buffer)) == 0)
                        { //verific conexiunea
                            if(c[j].connected == 1) {
                                // il marchez ca fiind conectat
                                printf("Clientul %s este deja conectat\n", buffer);
                                connected = 1;
                                s = 1;
                                // si il inchid
                                close(newsockfd);
                                break;
                            } else {
                                // daca nu e conectat, il reconectez
                                //il adaug la lista de clienti, dar si in lista de poll
                                connected = -1;
                                c[j].connected = 1;
                                c[j].sockfd = newsockfd;
                                fds[nfds].fd = newsockfd;
                                fds[nfds].events = POLLIN;
                                nfds++;
                                printf("New client %s connected from %s:%d.\n", buffer, inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));
                            }
                            if(s == 1) 
                            {
                                // daca e conectat, ii trimit mesajul de exit
                                msg messag;
                                memset(&messag, 0, sizeof(msg));
                                strcpy(messag.payload, "exit");
                                int rc = send(newsockfd, &messag, sizeof(msg), 0);
                                if(rc < 0) {
                                    perror("send");
                                    exit(-1);
                                }
                            }
                        } 
                    }
                    if(connected == 0) {
                        // daca nu e in lista de clienti, il adaug
                        printf("New client %s connected from %s:%d.\n", buffer, inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));
                        fds[nfds].fd = newsockfd;
                        fds[nfds].events = POLLIN;
                        nfds++;
                        
                        client new_client;
                        memset(&new_client, 0, sizeof(client));
                        strcpy(new_client.id, buffer);
                        new_client.connected = 1;
                        new_client.sockfd = newsockfd;
                        new_client.nr_topics = 0;
                        c = realloc(c, (nr_clienti + 1) * sizeof(client));
                        c[nr_clienti] = new_client;
                        nr_clienti++;
                    }

                    break;
                }
                else if(fds[i].fd == sockfdudp) 
                {
                    // daca am primit mesaj pe udp
					memset(buffer, 0, sizeof(msg));
                    socklen_t server_len = sizeof(server_addr);
					int n = recvfrom(sockfdudp, buffer, sizeof(msg), 0, (struct sockaddr *) &server_addr, &server_len);
                    if(n < 0) {
                        perror("recvfrom");
                        exit(-1);
                    }
                    msg mesi;
                    memset(&mesi, 0, sizeof(msg));
                    strcpy((char*)mesi.ip, inet_ntoa(server_addr.sin_addr));
                    mesi.type = buffer[50];
                    mesi.port = ntohs(server_addr.sin_port);
                    memcpy(&mesi.topic, buffer, 50);

                    uint8_t tip = buffer[50];
                    char payload[1501];
                    memcpy(payload, buffer + 51, 1500);
                    // am salvat mesajul in mesi, si continutul in payload
                    // in functie de tipul mesajului, il afisez

                    if(tip == 0) {
                        int val = ntohl(*(int*)(payload+1));
                        if((uint8_t)payload[0] == 1) {
                            val = (-1) * val;
                        }
                        sprintf(mesi.payload, "%d", val);
                    }else if(tip == 1) {
                        float val = ntohs(*(uint16_t*)(payload));
                        val = val / 100;
                        sprintf(mesi.payload, "%.2f", val);
                    } else if(tip == 2) {
                        float val = ntohl(*(int*)(payload+1));
                        if(payload[0] == 1) {
                            val = (-1) * val;
                        }
                        int i = 1;
                        for(int l = 0; l < payload[5]; l++) {
                            i = i * 10;
                        }
                        val = val / i;
                        if(payload[5] == 0) {
                            sprintf(mesi.payload, "%d", (int)val);
                        } else {
                            sprintf(mesi.payload, "%f", val);
                        }
                        if(mesi.payload[strlen(mesi.payload) - 1] == '0' && mesi.payload[strlen(mesi.payload) - 2] == '0' && mesi.payload[strlen(mesi.payload) - 3] == '0') {
                            mesi.payload[strlen(mesi.payload) - 1] = '\0';
                            mesi.payload[strlen(mesi.payload) - 1] = '\0';
                            mesi.payload[strlen(mesi.payload) - 1] = '\0';
                        } else if(mesi.payload[strlen(mesi.payload) - 1] == '0' && mesi.payload[strlen(mesi.payload) - 2] == '0') {
                            mesi.payload[strlen(mesi.payload) - 1] = '\0';
                            mesi.payload[strlen(mesi.payload) - 1] = '\0';
                        } else if(mesi.payload[strlen(mesi.payload) - 1] == '0') {
                            mesi.payload[strlen(mesi.payload) - 1] = '\0';
                        }
                    } else if(tip == 3) {
                        char val[1501];
                        strcpy(val, payload);
                        strcpy(mesi.payload, val);
                    }
                    // il trimit la toti clientii conectati
                    for(int j = 0; j < nr_clienti; j++) {
                        if(c[j].connected == 1) {
                            for(int k = 0; k < c[j].nr_topics; k++) {
                                if(strcmp(c[j].topics[k], mesi.topic) == 0) {
                                    int ret = send(c[j].sockfd, &mesi, sizeof(msg), 0);
                                    if(ret < 0) {
                                        perror("send");
                                        exit(-1);
                                    }
                                    break;
                                }
                            }
                        } else {
                            for(int k = 0; k < c[j].nr_topics; k++) {
                                if(strcmp(c[j].topics[k], mesi.topic) == 0) {
                                    c[j].sf[k] = 1;
                                    break;
                                    // daca nu e conectat, ii setez sf-ul pe 1
                                }
                            }
                        }
                    }
                    
                } else {
                    // primesc de la subscriber
                    msg mesaj;
                    memset(&mesaj, 0, sizeof(msg));
                    int ret = recv(fds[i].fd, &mesaj, sizeof(msg), 0);
                    if(ret < 0) {
                        perror("recv");
                        exit(-1);
                    }

                    if(ret == 0) {
                        // daca s-a deconectat
                        printf("Client %s disconnected.\n", buffer2);
                        for(int j = 0; j < nr_clienti; j++) {
                            if(strcmp(c[j].id, buffer2) == 0) {
                                c[j].connected = 0;
                                break;
                            }
                        }
                        close(fds[i].fd);
                        for(int j = i; j < nfds - 1; j++) {
                            fds[j] = fds[j + 1];
                        }
                        nfds--;
                    }
                    if(ret > 0) {
                        // primesc de la client
                        if(mesaj.command == 1) {
                            char topic[51];
                            strcpy(topic, mesaj.topic);
                            // am salvat topicul
                            
                        // subscribe
                            for(int j = 0; j < nr_clienti; j++) {
                                if(fds[i].fd == c[j].sockfd) {
                                    strcpy(c[j].topics[c[j].nr_topics], topic);
                                    c[j].nr_topics++;
                                    break;
                                }
                            }
                            
                        }
                        //unsubscribe
                        if(mesaj.command == 2) {
                            char topic[51];
                            strcpy(topic, mesaj.topic);
                            for(int j = 0; j < nr_clienti; j++) {
                                if(fds[i].fd == c[j].sockfd) {
                                    for(int k = 0; k < c[j].nr_topics; k++) {
                                        if(strcmp(c[j].topics[k], topic) == 0) {
                                            for(int l = k; l < c[j].nr_topics - 1; l++) {
                                                strcpy(c[j].topics[l], c[j].topics[l + 1]);
                                            }
                                            c[j].nr_topics--;
                                            break;
                                        }
                                    }
                                    break;
                                }
                            }
                        }
                        

                    }
                }
            }
        }
        if(flag == 0) {
            // exit
            break;
        }
    }
    // inchid toti socketii
    for(int i = 0; i < 100; i++) {
        if(fds[i].fd != -1)
            close(fds[i].fd);
    }
    close(sockfdtcp);
    close(sockfdudp);
    return 0;
}
