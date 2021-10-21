#include <stdio.h>		 	
#include <sys/socket.h>		
#include <arpa/inet.h>		
#include <stdlib.h>			
#include <string.h>			
#include <unistd.h>			
#include <errno.h>			
#include <signal.h>			

struct gbnpacket {
	int type;
	int seq_no;
	int length;
	char data[100];
};

#define TIMEOUT_SECS 3										
#define MAXTRIES 10										
#define MAX 100
#define PORT 8080

int tries = 0;												
int base = 0;
int windowSize;
int sendflag = 1;
int chunkSize = 10;

void CatchAlarm (int ignored) {
	tries += 1;
	sendflag = 1;
}

int max (int a, int b) {
	if (b > a)
		return b;
	return a;
}

int min(int a, int b) {
	if(b>a)
		return a;
	return b;
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
	if (connect (sock, (struct sockaddr*) &gbnServAddr, sizeof (gbnServAddr)) != 0)
			exit (0);

	char buffer[MAX];
	printf ("Please Enter your data to transmit: ");
	int i;
	for (i=0; (buffer [i] = getchar()) != '\n'; ++i);
	buffer [i] = '\0';
	printf ("Enter Window Size: ");
	scanf (" %d", &windowSize);
	const int datasize = MAX;													
	int nPackets = datasize / chunkSize;											
	if (datasize % chunkSize)
		nPackets++;
	struct sockaddr_in fromAddr;													
	unsigned int fromSize;															
	int respLen;																
	int packet_received = -1;														
	int packet_sent = -1;															  
	struct sigaction myAction;														
	myAction.sa_handler = CatchAlarm;
	if (sigfillset (&myAction.sa_mask) < 0)									
		exit (0);
	myAction.sa_flags = 0;
	if (sigaction (SIGALRM, &myAction, 0) < 0)
		exit (0);
	while ((packet_received < nPackets-1) && (tries < MAXTRIES)) {
		if (sendflag > 0) {
			sendflag = 0;
			int ctr; 																
			for (ctr = 0; ctr < windowSize; ctr++) {
				packet_sent = min(max (base + ctr, packet_sent),nPackets-1); 
				struct gbnpacket currpacket; 				
				if ((base + ctr) < nPackets) {
					bzero (&currpacket, sizeof(currpacket));
					printf ("SENDING PACKET %d PACKET SENT %d PACKET RECEIVED %d\n", base+ctr, packet_sent, packet_received);

					currpacket.type = htonl (1); 				
					currpacket.seq_no = htonl (base + ctr);
					int currlength;
					if ((datasize - ((base + ctr) * chunkSize)) >= chunkSize) 	
						currlength = chunkSize;
					else
						currlength = datasize % chunkSize;
					currpacket.length = htonl (currlength);
					memcpy (currpacket.data, 					
					buffer + ((base + ctr) * chunkSize), currlength);
					if (send (sock, &currpacket, (sizeof (int) * 3) + currlength, 0) != ((sizeof (int) * 3) + currlength))																	
						exit (0);
				}
			}
		}
		fromSize = sizeof (fromAddr);
		alarm (TIMEOUT_SECS);														
		struct gbnpacket currAck;
		while ((respLen = (recv (sock, &currAck, sizeof (int) * 3, 0))) < 0)																
			if (errno == EINTR)	{												
				if (tries < MAXTRIES) {											
					printf ("TIMED OUT, %d TRIES REMAIN\n", MAXTRIES - tries);
					break;
				}
				else
					exit (0);
			}
			else
				exit (0);
			if (respLen) {
				int acktype = ntohl (currAck.type); 								
				int ackno = ntohl (currAck.seq_no); 
				if (ackno > packet_received && acktype == 2) {
					printf ("RECEIVED ACK\n\n"); 								
					packet_received++;
					base = packet_received; 									
					if (packet_received == packet_sent) {					
						alarm (0); 													
						tries = 0;
						sendflag = 1;
					}
					else {															
						tries = 0; 													
						sendflag = 0;
						alarm(TIMEOUT_SECS); 										
					}
				}
			}
    }
	
	int ctr;
	for (ctr = 0; ctr < 10; ctr++) {											
		struct gbnpacket teardown;
		teardown.type = htonl (4);
		teardown.seq_no = htonl (0);
		teardown.length = htonl (0);
		send (sock, &teardown, (sizeof (int) * 3), 0);												
    }

	close (sock); 																
	return 0;
}
