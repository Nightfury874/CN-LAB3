#include <stdio.h>									
#include <sys/socket.h>									
#include <arpa/inet.h>									
#include <stdlib.h>									
#include <string.h>									
#include <unistd.h>									
#include <errno.h>										
#include <memory.h>
#include <signal.h>										

struct gbnpacket {
	int type;
	int seq_no;
	int length;
	char data[100];
};

#define MAX 100
#define PORT 8080
#define lossRate 0.5
#define chunkSize 10

void CatchAlarm (int ignored) {
	exit (0);
}

int main () {
	int sock = socket (AF_INET, SOCK_STREAM, 0);			
	struct sockaddr_in gbnServAddr;
	if (sock == -1)
		exit (0);
	bzero (&gbnServAddr, sizeof (gbnServAddr));
	gbnServAddr.sin_family = AF_INET;
	gbnServAddr.sin_addr.s_addr = htonl (INADDR_ANY);
	gbnServAddr.sin_port = htons (PORT);
	if (bind (sock, (struct sockaddr *) &gbnServAddr, sizeof (gbnServAddr)) != 0)
		exit (0);
	struct sockaddr_in gbnClntAddr;						
	unsigned int cliAddrLen;							
	if (listen (sock, 5) != 0)
		exit (0);			
	int connfd = accept (sock, (struct sockaddr*) &gbnClntAddr, &cliAddrLen);
	if (connfd < 0)
		exit (0);

	char buffer[MAX+1];
	buffer[MAX] = '\0';
	bzero (buffer, MAX);
	int recvMsgSize;									
	int packet_rcvd = -1;							
	srand48(123456789);				
	struct sigaction myAction;
	myAction.sa_handler = CatchAlarm;
	if (sigfillset (&myAction.sa_mask) < 0) 			
		exit (0);
	myAction.sa_flags = 0;
	if (sigaction (SIGALRM, &myAction, 0) < 0)
		exit (0);
	while (1) {							
		cliAddrLen = sizeof (gbnClntAddr);
		struct gbnpacket currPacket; 						
		bzero (&currPacket, sizeof(currPacket));
		recvMsgSize = recv (connfd, &currPacket, sizeof (currPacket), 0);		
		currPacket.type = ntohl (currPacket.type);
		currPacket.length = ntohl (currPacket.length); 		
		currPacket.seq_no = ntohl (currPacket.seq_no);
		if (currPacket.type == 4) { 					
			printf ("%s\n", buffer);
			struct gbnpacket ackmsg;
			ackmsg.type = htonl(8);
			ackmsg.seq_no = htonl(0);				
			ackmsg.length = htonl(0);
			if (send (connfd, &ackmsg, sizeof (ackmsg), 0) != sizeof (ackmsg))	
				exit (0);	
			alarm (7);
			while (1) {
				while ((recv (connfd, &currPacket, sizeof (int)*3+chunkSize, 0)) < 0)	
					if (errno == EINTR)					
						exit (0);					
				if (ntohl(currPacket.type) == 4) { 			
					ackmsg.type = htonl(8);
					ackmsg.seq_no = htonl(0);				
					ackmsg.length = htonl(0);
					if (send (connfd, &ackmsg, sizeof (ackmsg), 0) != sizeof (ackmsg))	
						exit (0);
				}
			}
			exit (0);
		}
		else {
			if(lossRate > drand48())
				continue; 									
			printf ("RECEIVE PACKET %d length %d\n", currPacket.seq_no, currPacket.length);
			if (currPacket.seq_no == packet_rcvd + 1) {
				packet_rcvd++;
				int buff_offset = chunkSize * currPacket.seq_no;
				memcpy (&buffer[buff_offset], currPacket.data, currPacket.length);
			}
	  
			printf ("SEND ACK %d\n\n", packet_rcvd);
			struct gbnpacket currAck; 						
			currAck.type = htonl (2); 					
			currAck.seq_no = htonl (packet_rcvd);
			currAck.length = htonl(0);
			if (send (connfd, &currAck, sizeof (currAck), 0) != sizeof (currAck))
				exit (0);
		}
    }
	
	printf ("SERVER EXIT\n");
	close (sock);
	return 0;
}
