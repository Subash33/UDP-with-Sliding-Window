# UDP-with-Sliding-Window

Author: Subash Khanal

## Description  
This program implements the sliding window protocol in UDP by transferring a string. The length of data is always 10 from the buﬀer. So the total size is actually ﬁxed at 22 bytes. The total number of packets that need to be received successfully at the receiver is 24. Their sequence numbers are 0, 1, 2, · · · 23. The two are two programs `sender.c` and `receiver.c` if run in two terminals within a same local machine (127.0.0.1) take part in the communication with sliding window protocol.  To simulate the loss of packets a sequence of packets to be dropped is passed as argument while running the server program.

## Compilation  
makefile is provided which can be run using command `$ make` in the working directory which contains the program for sender and receiver.

## Usage
After compliation two executables are available in the directory which can be run with the usage:  
`$ ./myclient <server_ip> <server_port>`  
`$ ./myserver <server_port> <List of numbers (limited to 10) passed as command line argument to simulate the packet loss>`

## Implementation Details
The client reads server ip, server port. The string to be sent is already initialized within the client program. The size of messages client will sent is sizeof(int)*3 + length. To simplify the assignment, we assume that the length of data is always 10. Every time we need to send 10 bytes from the buffer. So the total size is fixed at 22 bytes. The sequence numbers are 0,1,2,...23. The wait time for receieving ack is fixed at 4 secs.
The receiver declares a receiving buffer to hold the content sent from the sender. After receiving a packet from a sender, it will determine whether to drop the packet or not by checking with the list of numbers parameter provided as arguments when running the program. If the packet is not dropped cumulative ACK is sent back to the client.


