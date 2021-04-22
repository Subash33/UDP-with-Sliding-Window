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

#define ECHOMAX 10     /* Longest string to echo */
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
    
    struct data_pkt_t data_pkt;
    struct ack_pkt_t ack_pkt;

    if ((argc < 2) || (argc > 3)) /* Test for correct number of arguments */
    {
        fprintf(stderr, "Usage: %s <Server IP> [<Server Port>]\n", argv[0]);
        exit(1);
    }

    servIP = argv[1]; /* First arg:  server IP address (dotted quad) */
    if (argc > 2)
        echoServPort = atoi(argv[2]); /* Use given port, if any */
    else
        echoServPort = 7; /* 7 is well-known port for echo service */

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
    int numBytes = -1;        
    int lastAckReceived = -1; 
    /* 
    * sliding window initialization
    */
    int static const slidingWindowSize = 5;
    int static const numPackets = 24; //number of packets to create and send

    int static const totalClosingCount = 24;

    /*
    acknowledgement variables
    */
    int static const ackLength = 7;      
    int static const ackSeqCumLength = 4; 

    int i = 0;
    int j = 0;
    int ndups = 0;

    char closingCount = 0; //This needed for tracking final ACK lost

    
    fromSize = sizeof(fromAddr);

    while (1)
    {
        for (i = lastAckReceived + 1; i <= (lastAckReceived + slidingWindowSize) && i <= numPackets; i++)
        {

            if (i == numPackets)
            {

                closingCount++;
            }

            int packetSize = createPacketStruct(&data_pkt, i, ECHOMAX, echoString);

            now = time(0);
            numBytes = sendto(sock, &data_pkt, packetSize, 0, (struct sockaddr *)&echoServAddr, sizeof(echoServAddr));
            printf("SEND PACKET %d\n", data_pkt.seq_num);
        }
        if (closingCount >= totalClosingCount)
        {
            printf("Final ACK Lost.....Ending!\n");
            break;
        }

        //Client Listening to server's ACK begins //
        while (1)
        {
            
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
                    if (i == numPackets && cumAckNum == numPackets)
                    {
                        printf("Data Transfer Completed!\n");
                        close(sock);
                        exit(0);
                    }

                    if (i == numPackets && cumAckNum < numPackets && j > 0)
                    {
                        if (cumAckNum == numPackets - 1)
                        {
                            closingCount++;
                        }

                        int packetSize = createPacketStruct(&data_pkt, i, ECHOMAX, echoString);

                        now = time(0);
                        numBytes = sendto(sock, &data_pkt, packetSize, 0, (struct sockaddr *)&echoServAddr, sizeof(echoServAddr));
                        printf("RESEND Packet %d\n", data_pkt.seq_num);
                    }

                    //If ack = base-1 OR duplicate ack count equals 3 RESEND
                    if (cumAckNum - 1 == lastAckReceived && cumAckNum < numPackets)
                    {
                        ndups++;
                        if (ndups == 3)
                        {
                            j++;
                            //for final packet
                            if (cumAckNum == numPackets - 1)
                            {
                                closingCount++;
                            }

                            int packetSize = createPacketStruct(&data_pkt, i, ECHOMAX, echoString);

                            now = time(0);
                            numBytes = sendto(sock, &data_pkt, packetSize, 0, (struct sockaddr *)&echoServAddr, sizeof(echoServAddr));
                            printf("RESEND Packet %d\n", data_pkt.seq_num);
                            ndups =0;
                        }
                    }

                    //Normal case: if ack >= base :Send next packet
                    if (cumAckNum - 1 > lastAckReceived && cumAckNum < numPackets && i < numPackets)
                    {
                        lastAckReceived++;
                        //for final packet
                        if (cumAckNum == numPackets - 1 && i == numPackets - 1)
                        {
                            closingCount++;
                        }
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
                    tries += 1; 
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
