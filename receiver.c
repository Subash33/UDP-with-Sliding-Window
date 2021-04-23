/**
 * server code
 * Reliable Data Transfer Protocol
 * Author: Subash Khanal
 * 
**/

#include <stdio.h>      /* for printf() and fprintf() */
#include <sys/types.h>
#include <sys/socket.h> /* for socket() and bind() */
#include <netinet/in.h>
#include <arpa/inet.h>  /* for sockaddr_in and inet_ntoa() */
#include <netdb.h>
#include <time.h>
#include <stdlib.h>     /* for atoi() and exit() */
#include <string.h>     /* for memset() */
#include <unistd.h>     /* for close() */

#define ECHOMAX 10 /* Longest string to echo */

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

void DieWithError(char *errorMessage) /* External error handling function */
{
    perror(errorMessage);
    exit(1);
}
void stripHeader(char packet[], char head[]);

int main(int argc, char *argv[])
{
    int sock;                        /* Socket */
    struct sockaddr_in echoServAddr; /* Local address */
    struct sockaddr_in echoClntAddr; /* Client address */
    unsigned int cliAddrLen;         /* Length of incoming message */
    char echoBuffer[ECHOMAX];        /* Buffer for echo string, replaced by rcvMsg */
    unsigned short echoServPort;     /* Server port */
    int dropNum[10];
    int c = 0;

    if ((argc > 12) || (argc < 2)) /* Test for correct number of parameters */
    {
        fprintf(stderr, "Usage:  %s <SERVER PORT> <FORCE DROP NUMBERS(upto 10)>\n", argv[0]);
        exit(1);
    }

    echoServPort = atoi(argv[1]); /* First arg:  local port */
    if (argc > 2)
    {
        for (int i = 2; i < argc; i++)
        {
            dropNum[i - 2] = atoi(argv[i]);
        }
    }

    /* Create socket for sending/receiving datagrams */
    if ((sock = socket(PF_INET, SOCK_DGRAM, IPPROTO_UDP)) < 0)
        DieWithError("socket() failed");

    /* Construct local address structure */
    memset(&echoServAddr, 0, sizeof(echoServAddr));   /* Zero out structure */
    echoServAddr.sin_family = AF_INET;                /* Internet address family */
    echoServAddr.sin_addr.s_addr = htonl(INADDR_ANY); /* Any incoming interface */
    echoServAddr.sin_port = htons(echoServPort);      /* Local port */

    memset(&echoClntAddr, 0, sizeof(echoClntAddr)); /* Zero out structure */

    /* Bind to the local address */
    if (bind(sock, (struct sockaddr *)&echoServAddr, sizeof(echoServAddr)) < 0)
        DieWithError("bind() failed");

    int static const dataLength = 1024;

    int numPackets = 24;

    time_t now;
    int numBytes;
    char payload[dataLength];
    char file[15000] = {0};
    int filesize;
    int cumAckWindow[1000];

    int z = 0;
    for (z = 0; z < sizeof(cumAckWindow); z++)
    {
        cumAckWindow[z] = 0;
    }

    int receiveWindowSize = 5;
    int lastFrameReceived = -1;

    struct data_pkt_t r_data_pkt;
    struct ack_pkt_t ack_pkt;

    while (1)
    {

        memset(&payload, 0, sizeof(payload));
        cliAddrLen = sizeof(echoClntAddr);

        numBytes = recvfrom(sock, &r_data_pkt, sizeof(r_data_pkt), 0, (struct sockaddr *)&echoClntAddr, &cliAddrLen);

        int sequenceNumber = r_data_pkt.seq_num;
        int length = r_data_pkt.length;
        memcpy(payload, r_data_pkt.data, length);

        int ack;
        int ack_cum;

        //Check if the packet is to be dropped
        if (sequenceNumber == dropNum[c] && c < 10 && argc > 2)
        {
            printf("RECEIVE Packet %d \n ---- DROP %d\n", sequenceNumber, sequenceNumber);
            now = time(0);
            c += 1;

            for (int i = 0; i < sizeof(cumAckWindow) / sizeof(int); i++)
            {
                if (cumAckWindow[i] == 0)
                {
                    ack_cum = i;
                    break;
                }
            }

            ack = lastFrameReceived;
        }

        else
        {
            printf("RECEIVE PACKET %d\n", sequenceNumber);

            now = time(0);

            /* ACK packet prepartion*/
            if (sequenceNumber >= lastFrameReceived && sequenceNumber <= lastFrameReceived + 1 + receiveWindowSize)
            {
                cumAckWindow[sequenceNumber] = 1;
                memcpy(&file[sequenceNumber * ECHOMAX], payload, length);
                ack = sequenceNumber;
                for (int i = 0; i < sizeof(cumAckWindow) / sizeof(int); i++)
                {
                    if (cumAckWindow[i] == 0)
                    {
                        ack_cum = i;
                        break;
                    }
                }
                lastFrameReceived = ack_cum - 1;
            }
            else
            {

                for (int i = 0; i < sizeof(cumAckWindow) / sizeof(int); i++)
                {
                    if (cumAckWindow[i] == 0)
                    {
                        ack_cum = i;
                        break;
                    }
                }
                ack = lastFrameReceived;
            }

            /* Prepare ACK */

            ack_pkt.type = 2;
            ack_pkt.ack_no = ack;
            ack_pkt.ack_cum_no = ack_cum;

            printf("-------- SEND ACK %d\n", ack_cum - 1);

            now = time(0);
            if (sendto(sock, &ack_pkt, sizeof(ack_pkt), 0,
                       (struct sockaddr *)&echoClntAddr, sizeof(echoClntAddr)) != sizeof(ack_pkt))
            {
                DieWithError("sendto() sent a different number of bytes than expected");
            }

            //last packet received here
            if (ack_cum == numPackets)
            {
                printf("--- RECEIVE MESSAGE : %s\n", file);
                filesize = ((sequenceNumber - 1) * ECHOMAX) + length;

                FILE *outFilePtr;
                outFilePtr = fopen("receivedFile.txt", "wb");

                if (fwrite(file, 1, filesize, outFilePtr) != filesize || outFilePtr == NULL)
                {
                    printf("Error writing to file!\n");
                }
                fclose(outFilePtr);
                break;
            }
        }
    }
    /*Communication Over*/
    close(sock);
    return 0;
}
