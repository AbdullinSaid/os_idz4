#include <stdio.h>      /* for printf() and fprintf() */
#include <sys/socket.h> /* for socket(), bind(), and connect() */
#include <arpa/inet.h>  /* for sockaddr_in and inet_ntoa() */
#include <stdlib.h>     /* for atoi() and exit() */
#include <string.h>     /* for memset() */
#include <unistd.h>  /* TCP echo server includes */
#include <pthread.h>        /* for POSIX threads */
#include <sys/sem.h>
#include <sys/shm.h>
#include <sys/ipc.h>
#include <sys/wait.h>

void *ThreadMain(void *arg);
 
#define MAXPENDING 5
 
void DieWithError(char *errorMessage)
{
    perror(errorMessage);
    exit(1);
}
 
int CreateUPDServerSocket(unsigned short port)
{
    int sock;
    struct sockaddr_in servAddr;
 
    if ((sock = socket(PF_INET, SOCK_DGRAM, IPPROTO_UDP)) < 0)
        DieWithError("socket() failed");
 
    memset(&servAddr, 0, sizeof(servAddr));
    servAddr.sin_family = AF_INET;
    servAddr.sin_addr.s_addr = htonl(INADDR_ANY);
    servAddr.sin_port = htons(port);

    if (bind(sock, (struct sockaddr *) &servAddr, sizeof(servAddr)) < 0)
        DieWithError("bind() failed");
 
    return sock;
}

int CreateUPDServerMonitorSocket(unsigned short port, char *multicastIP)
{
    int sock;
  
    unsigned char multicastTTL = 1;
    struct sockaddr_in multicastAddr;
 
    if ((sock = socket(PF_INET, SOCK_DGRAM, IPPROTO_UDP)) < 0)
        DieWithError("socket() failed");

    /* Set TTL of multicast packet */
    if (setsockopt(sock, IPPROTO_IP, IP_MULTICAST_TTL, (void *) &multicastTTL,
          sizeof(multicastTTL)) < 0)
        DieWithError("setsockopt() failed");

    /* Construct local address structure */
    memset(&multicastAddr, 0, sizeof(multicastAddr));   /* Zero out structure */
    multicastAddr.sin_family = AF_INET;                 /* Internet address family */
    multicastAddr.sin_addr.s_addr = inet_addr(multicastIP);/* Multicast IP address */
    multicastAddr.sin_port = htons(multicastPort);
 
    return sock;
}

#define MAX_BOOKS 50
#define MAX_BUFFER_SIZE 100000
#define RCVBUFSIZE 64

static int semBooks[MAX_BOOKS];
static int semMonitorBuffer;

static char monitorBuffer[MAX_BUFFER_SIZE];
static int monitorBufferSize = 0;
static int booksNum;

struct sembuf sem_wait = {0, -1, SEM_UNDO};
struct sembuf sem_signal = {0, 1, SEM_UNDO};

void clean_all() {
    for (int i = 0; i < booksNum; ++i) {
         semctl(semBooks[i], 0, IPC_RMID, 0);
    }
}

void sigint_handler(int signum) {
    clean_all();
    exit(-1);
}


int main(int argc, char *argv[])
{
    int servSock1;
    int servSock2;
    int clntSock;
    unsigned short servPort1;
    unsigned short servPort2;
    char *multicastIP;
    pthread_t threadID;
    struct sockaddr_in echoClntAddr;
    unsigned int cliAddrLen;       

    if (argc != 5)
    {
        fprintf(stderr,"Usage: %s <SERVER PORT 1> <SERVER PORT 2> <NUMBER OF BOOKS> <Multicast Address>\n", argv[0]);
        exit(1);
    }

    signal(SIGINT, sigint_handler);
    signal(SIGTERM, sigint_handler);
    
    servPort1 = atoi(argv[1]);
    servPort2 = atoi(argv[2]);
    booksNum = atoi(argv[3]);
    multicastIP = argv[4];

    if ((semMonitorBuffer = semget(123, 1, IPC_CREAT | 0666)) == -1) {
        perror("semget");
        exit(-1);
    }
    if (semctl(semMonitorBuffer, 0, SETVAL, 1) == -1) {
        perror("semctl");
        exit(-1);
    }
    for (int i = 0; i < booksNum; ++i) {
        if ((semBooks[i] = semget(1234 * (i + 1), 1, IPC_CREAT | 0666)) == -1) {
            perror("semget");
            exit(-1);
        }
        if (semctl(semBooks[i], 0, SETVAL, 1) == -1) {
            perror("semctl");
            exit(-1);
        }
    }
    monitorBufferSize = 0;
    servSock1 = CreateUPDServerSocket(servPort1);
    servSock2 = CreateUPDServerMonitorSocket(servPort2, multicastIP);

  
		for (int j = 1; i <= 10; ++i) {
		    if ((childpid = fork()) == 0) {
            for (;;)
            {
                cliAddrLen = sizeof(echoClntAddr);
                /* Block until receive message from a client */
                if ((recvMsgSize = recvfrom(servSock1, echoBuffer, ECHOMAX, 0,
                    (struct sockaddr *) &echoClntAddr, &cliAddrLen)) < 0)
                    DieWithError("recvfrom() failed");
                int requiredBook = atoi(recvMsgSize);
                char sendString[100];
                int sendStringLen;
                sendStringLen = snprintf(sendString, "Client %d required book number %d\n", echoClntAddr.sin_addr, requiredBook);
                printf("Client %d required book number %d\n", echoClntAddr.sin_addr, requiredBook);
                if (sendto(servSock2, sendString, sendStringLen, 0, (struct sockaddr *)
                    &multicastAddr, sizeof(multicastAddr)) != sendStringLen)
                    DieWithError("sendto() sent a different number of bytes than expected");

                semop(semBooks[requiredBook], &sem_wait, 1);
                sendStringLen = snprintf(sendString, "Client %d took book number %d\n", echoClntAddr.sin_addr, requiredBook);
                printf("Client %d took book number %d\n", echoClntAddr.sin_addr, requiredBook);
                if (sendto(servSock2, sendString, sendStringLen, 0, (struct sockaddr *)
                    &multicastAddr, sizeof(multicastAddr)) != sendStringLen)
                    DieWithError("sendto() sent a different number of bytes than expected");
                sleep(10);
                sendStringLen = snprintf(sendString, "Client %d took book number %d\n", echoClntAddr.sin_addr, requiredBook);
                printf("Client %d took book number %d\n", echoClntAddr.sin_addr, requiredBook);
                if (sendto(servSock2, sendString, sendStringLen, 0, (struct sockaddr *)
                    &multicastAddr, sizeof(multicastAddr)) != sendStringLen)
                    DieWithError("sendto() sent a different number of bytes than expected");
                semop(semBooks[requiredBook], &sem_signal, 1);
               
                /* Send received datagram back to the client */
                if (sendto(servSock1, "home", 4, 0,
                    (struct sockaddr *) &echoClntAddr, sizeof(echoClntAddr)) != recvMsgSize)
                    DieWithError("sendto() sent a different number of bytes than expected");

                printf("Client %d walked home\n", echoClntAddr.sin_addr);
                sendStringLen = snprintf(sendString, "Client %d walked home\n", echoClntAddr.sin_addr);
                if (sendto(servSock2, sendString, sendStringLen, 0, (struct sockaddr *)
                    &multicastAddr, sizeof(multicastAddr)) != sendStringLen)
                    DieWithError("sendto() sent a different number of bytes than expected");
            }
        }
    }
    /* NOT REACHED */
}
