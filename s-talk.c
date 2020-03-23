//PThread referenced from https://computing.llnl.gov/tutorials/pthreads/
//                        and Tutorial #4 from coursys
//Socket Programming referenced from https://www.cs.rutgers.edu/~pxk/417/notes/sockets/udp.html , 
//                                   http://beej.us/guide/bgnet/ ,
//                                   and Tutorial #3 from coursys
//Implemented with list.h and list.o provided by the instructor


#include <stdio.h>           //CMPT 300 Assignment 2
#include <stdlib.h>          //Yoonhong Lee, 301267876
#include <pthread.h>         //February 27th, 2020
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <arpa/inet.h> 
#include <netdb.h> 
#include "list.h"

#define MAX_MSG_LEN 1024  //Maximum msg length


int port; //User's port
int rport; //Remote user's port

LIST* sent_msg;     //Queues msg for sending
LIST* recieved_msg; //Queues received msg
int sock;           
int control = 0;    //Control variable for cancelling all threads

pthread_t tids[4];   //Threads for the send_msg, receive_msg, keyboard_input, display_msg
pthread_mutex_t receivingMutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t sendingMutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t receivingCond = PTHREAD_COND_INITIALIZER;
pthread_cond_t sendingCond = PTHREAD_COND_INITIALIZER;

//Thread Function for sending msg
void* send_msg(void* addr) {
	char *msg;
	struct sockaddr_in address = *(struct sockaddr_in*) addr;
	while(1) {
		if(control == 1 && ListCount(sent_msg) == 0) {    //Checks if Control variable is on and no sending msgs are queued
			break;
		} else {
			pthread_mutex_lock(&sendingMutex);  
			if(ListCount(sent_msg) == 0) {     //Checks if there are msgs in the LIST still needs to be dequeued
				pthread_cond_wait(&sendingCond, &sendingMutex); 
			}	
			msg = ListTrim(sent_msg);   //Critical Section
			
			//pthread_cond_signal(&sendingCond);
			pthread_mutex_unlock(&sendingMutex);		
			sendto(sock, msg, MAX_MSG_LEN, 0,  (struct sockaddr *)&address, sizeof(address));
		}
	}

	pthread_exit(NULL);  //End the thread process
}

//Thread Function for receiving msg
void* receive_msg(void* addr) {
	char msg[MAX_MSG_LEN];
	struct sockaddr_in address = *(struct sockaddr_in*) addr;
	socklen_t addr_len;
	while(1) {
		if(control == 1) {  //Exit loop if the Control variable is on
			break;
		} else {
			recvfrom(sock, msg, 1024,  0,(struct sockaddr *)&address, &addr_len);  
			pthread_mutex_lock(&receivingMutex);
			
			ListPrepend(recieved_msg, msg);   //Critical Section
			
			pthread_cond_signal(&receivingCond);
			pthread_mutex_unlock(&receivingMutex);
			if(msg[0] == '!' && strlen(msg) == 2) {   //if ! is received then activates the Control variable then leaves the loop
				control = 1;
				break;
			}
		}
	}

	pthread_cancel(tids[2]);   //Signal keyboard_input thread to end
	pthread_cancel(tids[0]);
	pthread_exit(NULL);   //End this thread process
}

//Thread Function for keyboard input for user
void* keyboard_input(void* input) {
	char msg[MAX_MSG_LEN];
	while(1) {
		if(control == 1) {  //Exit loop if the Control variable is on
			break;
		} else {
			fgets(msg, MAX_MSG_LEN, stdin);    //Gets the user input
			pthread_mutex_lock(&sendingMutex);
			
			ListPrepend(sent_msg, msg);     //Critical Section

			pthread_cond_signal(&sendingCond);
			pthread_mutex_unlock(&sendingMutex);
			if(msg[0] == '!' && strlen(msg) == 2) {  //if ! is typed then activates the Control variable then leaves the loop
				printf("You have terminated connection\n");
				control = 1;
				break;
			}
		}
	}
	pthread_cancel(tids[1]);  //Signal receive_msg thread to end
	pthread_cancel(tids[3]);  //Signal receive_msg thread to end
	pthread_exit(NULL);  //End this thread process
}

//Thread Function for printing msg
void* display_msg(void* str) {
	char *msg;
	char remote[] = "Remote: ";
	while(1) {
		if(control == 1 && ListCount(recieved_msg) == 0) {  //Checks if Control variable is on and no recieved msgs are queued
			break;
		} else {
			pthread_mutex_lock(&receivingMutex);
			if(ListCount(recieved_msg) == 0) {
				pthread_cond_wait(&receivingCond, &receivingMutex);		
			}	
			msg = ListTrim(recieved_msg);     //Critical Section 
			
			//pthread_cond_signal(&receivingCond);
			pthread_mutex_unlock(&receivingMutex);
			fputs(remote, stdout);
			if(msg[0] == '!' && strlen(msg) == 2) {  //if ! is in the queued print different msg
				printf("Remote has terminated connection\n");
			} else {
				fputs(msg, stdout);
			}
		}
	}
	pthread_exit(NULL);  //End this thread process
}

int main(int argc, char *argv[]) {
	//Ports and hostName inputs saved from the terminal arguments
	port = atoi(argv[1]);    
	char* remoteAddr = argv[2];
	rport = atoi(argv[3]);


	sent_msg = ListCreate();
	recieved_msg = ListCreate();


	sock = socket(AF_INET, SOCK_DGRAM, 0);  //Socket for UDP initialized
	
	struct sockaddr_in master_addr;
	master_addr.sin_family = AF_INET; //Set to IPv4
	master_addr.sin_port = htons(port); //Set a port
	master_addr.sin_addr.s_addr = INADDR_ANY; //Set to local address
	memset(&master_addr.sin_zero, '\0', sizeof(master_addr));
	bind(sock, (struct sockaddr *)&master_addr, sizeof(struct sockaddr_in));


	struct hostent *remoteHost = gethostbyname(remoteAddr); //Get the address info from the hostname
	struct sockaddr_in remote_addr;
	remote_addr.sin_family = AF_INET;  //Set to IPv4
	remote_addr.sin_port = htons(rport);  //Set a port
	remote_addr.sin_addr = *((struct in_addr *) remoteHost->h_addr);  //Set the address to corresponding remote hostname 


	pthread_attr_t attribute;
	pthread_attr_init(&attribute);

	pthread_create(&tids[0], &attribute, send_msg, &remote_addr);  //Thread for send_msg()
	pthread_create(&tids[1], &attribute, receive_msg, &master_addr);  //Thread for receive_msg()
	pthread_create(&tids[2], &attribute, keyboard_input, NULL);  //Thread for keyboard_input()
	pthread_create(&tids[3], &attribute, display_msg, NULL);  //Thread for display_msg()

	for(int i = 0; i < 4; i++) {  //Joining back all the Threads back to one after all the thread processes have ended
		pthread_join(tids[i], NULL);
	}


	close(sock);  //Close the socket
	return 0;    //End of the program
}