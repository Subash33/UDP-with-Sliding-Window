/**
 * client code
 * Reliable Data Transfer Protocol
 * Author: Subash Khanal
 * 
**/

#include <stdio.h>      /* for printf() and fprintf() */
#include <sys/socket.h> /* for socket(), connect(), sendto(), and recvfrom() */
#include <arpa/inet.h>  /* for sockaddr_in and inet_addr() */
#include <stdlib.h>     /* for atoi() and exit() */
#include <string.h>     /* for memset() */
#include <unistd.h>     /* for close() and alarm() */
#include <errno.h>      /* for errno and EINTR */
#include <signal.h>     /* for sigaction() */
#include <pthread.h>
#include <time.h>
#include <sys/types.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <netdb.h>

#define ECHOMAX 10     /* Longest string to echo*/
#define TIMEOUT_SECS 4 /* Seconds between retransmits */
#define MAXTRIES 5     /* Tries before giving up */

int tries = 0; /* Count of times sent - GLOBAL for signal-handler access */

struct data_pkt_t
{
    int type;
    int seq_num;
    int length;
    char *data[ECHOMAX];
};

struct ack_pkt_t
{
    int type;
    int ack_no;
    int ack_cum_no;
};

void DieWithError(char *errorMessage) /* Error handling function */
{
    perror(errorMessage);
    exit(1);
}

void CatchAlarm(int ignored) /* Handler for SIGALRM */
{
    tries += 1;
}

int createPacketStruct(struct data_pkt_t *dataPacket, int seq, int length, char *echoString)
{
    //reset packet data
    memset(dataPacket->data, 0, length);

    //create packet
    dataPacket->type = 1;
    dataPacket->seq_num = seq;
    dataPacket->length = length;
    int beginIndex = seq * length;

    char data[length];
    memcpy(dataPacket->data, echoString + beginIndex, length);
    memset(data, 0, length);
    memcpy(data, dataPacket->data, length);
    int sizeTotal = sizeof(dataPacket->type) + sizeof(dataPacket->seq_num) + sizeof(dataPacket->length) + sizeof(dataPacket->data); //length;
    return sizeTotal;
}

int main(int argc, char *argv[])
{
    int sock;                        /* Socket descriptor */
    struct sockaddr_in echoServAddr; /* Echo server address */
    struct sockaddr_in fromAddr;     /* Source address of echo */
    unsigned short echoServPort;     /* Echo server port */
    unsigned int fromSize;           /* In-out of address size for recvfrom() */
    socklen_t servAddrSize;
    struct sigaction myAction;    /* For setting signal handler */
    char *servIP;                 /* IP address of server */
    char *echoString;             /* String to send to echo server */
    char echoBuffer[ECHOMAX + 1]; /* Buffer for echo string */
    int echoStringLen;            /* Length of string to echo */
    int respStringLen;            /* Size of received datagram */

    struct data_pkt_t data_pkt;
    struct ack_pkt_t ack_pkt;

    if ((argc < 2) || (argc > 4)) /* Test for correct number of arguments */
    {
        fprintf(stderr, "Usage: %s <Server IP> [<Server Port>] <Echo Word>\n", argv[0]);
        exit(1);
    }

    servIP = argv[1]; /* First arg:  server IP address (dotted quad) */
    if (argc > 2)
        echoServPort = atoi(argv[2]); /* Use given port, if any */
    else
        echoServPort = 7; /* 7 is well-known port for echo service */

    if (argc == 4)
        echoString = argv[3]; /* Second arg: string to echo */
    else
        echoString = "The University of Kentucky is a public, research-extensive, land grant university dedicated to improving people’s lives through excellence in teaching, research, health care, cultural enrichment, and economic development.";

    /* Create a best-effort datagram socket using UDP */
    if ((sock = socket(PF_INET, SOCK_DGRAM, IPPROTO_UDP)) < 0)
        DieWithError("socket() failed");

    /* Set signal handler for alarm signal */
    myAction.sa_handler = CatchAlarm;
    if (sigfillset(&myAction.sa_mask) < 0) /* block everything in handler */
        DieWithError("sigfillset() failed");
    myAction.sa_flags = 0;

    if (sigaction(SIGALRM, &myAction, 0) < 0)
        DieWithError("sigaction() failed for SIGALRM");

    /* Construct the server address structure */
    memset(&echoServAddr, 0, sizeof(echoServAddr)); /* Zero out structure */
    echoServAddr.sin_family = AF_INET;
    echoServAddr.sin_addr.s_addr = inet_addr(servIP); /* Server IP address,127.0.0.1 */
    echoServAddr.sin_port = htons(echoServPort);      /* Server port */

    /* Construct the client address structure*/
    memset(&fromAddr, 0, sizeof(fromAddr));
    fromAddr.sin_family = AF_INET;
    fromAddr.sin_addr.s_addr = htonl(INADDR_ANY); /* Client IP address, localhost */
    fromAddr.sin_port = htons(0);                 /* Server port*/

    /* Bind to the local address*/
    if (bind(sock, (struct sockaddr *)&fromAddr, sizeof(fromAddr)) < 0)
        DieWithError("bind() failed");

    time_t now;
    int numBytes = -1;        //number of bytes sent
    int lastAckReceived = -1; //last ack received
    /* 
    * sliding window initialization
    */
    int static const slidingWindowSize = 5;
    int static const numPackets = 24; //number of packets to create and send
    
    int i = 0;
    int j = 0;
    int ndups = 0;


    /* Get a response */
    fromSize = sizeof(fromAddr);

    while (1)
    {
        for (i = lastAckReceived + 1; i <= (lastAckReceived + slidingWindowSize) && i <= numPackets; i++)
        {


            int packetSize = createPacketStruct(&data_pkt, i, ECHOMAX, echoString);
            // printf("size of packet: %d\n", packetSize);

            now = time(0);
            numBytes = sendto(sock, &data_pkt, packetSize, 0, (struct sockaddr *)&echoServAddr, sizeof(echoServAddr));
            printf("SEND PACKET %d\n", data_pkt.seq_num);
        }


        /* Listen for ack from reciever */
        while (1)
        {
            //Setup Select
            struct timeval tv;
            fd_set set;

            //set timeout
            tv.tv_sec = TIMEOUT_SECS;
            tv.tv_usec = 0;
            FD_ZERO(&set);
            FD_SET(sock, &set);

            if (select(sock + 1, &set, NULL, NULL, &tv) < 0)
            {
                perror("Select error");
                exit(0);
            }

            if (FD_ISSET(sock, &set))
            {
                //process ack here
                if (recvfrom(sock, &ack_pkt, sizeof(ack_pkt), 0, (struct sockaddr *)&echoServAddr, &servAddrSize) < 0)
                {
                    if (tries < MAXTRIES)
                    {
                        tries += 1; //need to be checked at last
                        printf("Timed Out. %d more tries ...\n", MAXTRIES - tries);
                        sleep(1);
                    }
                    else
                    {
                        printf("TIMEOUT. Resend the packet.\n");
                        DieWithError("No Response");
                    }
                }
                else
                {

                    int ackNum = ack_pkt.ack_no;
                    int cumAckNum = ack_pkt.ack_cum_no;
                    printf("-------- RECEIVE ACK %d \n", cumAckNum - 1);
                    now = time(0);

                    //done
                    if (i == numPackets && cumAckNum == numPackets)
                    {
                      
                        printf("Data Transfer Completed!\n");
                        close(sock);
                        exit(0);
                    }

                    //Resend the packet if ack = base-1 i.e. duplicate ack with count equal to 3
                    if (cumAckNum - 1 == lastAckReceived && cumAckNum < numPackets)
                    {
                        ndups++;
                        if (ndups == 3)
                        {
                            j++;
                            //for final packet
                            
                            int packetSize = createPacketStruct(&data_pkt, cumAckNum, ECHOMAX, echoString);

                            now = time(0);
                            numBytes = sendto(sock, &data_pkt, packetSize, 0, (struct sockaddr *)&echoServAddr, sizeof(echoServAddr));
                            printf("RESEND Packet %d\n", data_pkt.seq_num);
                        }
                    }

                    //Send the next packet if ack > base
                    if (cumAckNum - 1 > lastAckReceived && cumAckNum < numPackets && i < numPackets)
                    {
                        ndups = 0;
                        lastAckReceived++;
                        
                        int packetSize = createPacketStruct(&data_pkt, i, ECHOMAX, echoString);

                        now = time(0);
                        numBytes = sendto(sock, &data_pkt, packetSize, 0, (struct sockaddr *)&echoServAddr, sizeof(echoServAddr));
                        printf("SEND Packet %d\n", data_pkt.seq_num);
                        i++;
                    }
                }
            }
            else
            {
                if (tries < MAXTRIES)
                {
                    tries += 1; //need to be checked at last
                    printf("TIMEOUT. Resend window!\n");
                    printf("%d more tries ...\n", MAXTRIES - tries);
                   
                }
                else
                {
                    printf("TIMEOUT. Closing the program...\n");
                    DieWithError("No Response");
                }
                break;
            }
        }
    }
}
