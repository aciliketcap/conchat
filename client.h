/*
 * client.h
 * Copyright 2012 Sinan Akpolat
 *
 * A chat client to work with my chat server
 *
 * This file is distributed under GNU GPLv3, see LICENSE file.
 * If you haven't received a file named LICENSE see <http://www.gnu.org/licences>
 *
 * This client is distributed WITHOUT ANY WARRANTY;
 * without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE
 */

#ifndef CLIENT_H_
#define CLIENT_H_

#include <unistd.h> //for getopt()
#include <stdlib.h> //for malloc()
#include <string.h> //for strncpy()
#include <stdio.h>
#include <errno.h> //for perror

#include <netdb.h> //for getaddrinfo() and struct addrinfo

#include <sys/socket.h> //socket structs and functions
#include <sys/types.h> //socket types and address families
#include <netinet/in.h> //some systems use this instead of arpa/inet.h
#include <arpa/inet.h> //IP related structs and functions

#include <pthread.h>

#include "messagelog.h"

#define MAX_ADDR_LEN 256

#define MSG_LEN 256
#define MSG_LOG_LEN 1024

extern int debug_mode; //1 true, 0 false

extern struct messagelog climsglog;
pthread_mutex_t climsglog_mutex = PTHREAD_MUTEX_INITIALIZER;

extern char *recvDelta;

extern pthread_t send_thread;
extern pthread_t recv_thread;

struct clientopts {
	int connectPortInt;	//The port we will connect to.
	char connectAddrString[MAX_ADDR_LEN]; //The address we will connect to.
};

int getcliopts(int argc, char **argv, struct clientopts *cliopt) {
	int c;
	while ((c = getopt(argc, argv, "a:p:d")) != -1) {
		switch(c) {
		case 'a':
			if(!optarg) {
				printf("You need to enter an address!\n");
				return 0;
			}
			strncpy(cliopt->connectAddrString, optarg, MAX_ADDR_LEN);
			if(cliopt->connectAddrString[MAX_ADDR_LEN - 1] != '\0') { //which means address string overflowed
				fprintf(stderr, "Address is too long!\n");
				return 0;
			}
			else printf("Server address is set to: %s\n", cliopt->connectAddrString);
			break;
		case 'p':
			if(!optarg) {
				printf("You need to enter port number!\n");
				return 0;
			}
			cliopt->connectPortInt = 0;
			if(cliopt->connectPortInt = atoi(optarg)) {
				if(cliopt->connectPortInt < 0 || cliopt->connectPortInt > 65535) {
					fprintf(stderr, "Server port should be between 0 and 65535.\n");
					return 0;
				}
				else {
					printf("Server port number set to %d.\n", cliopt->connectPortInt);
				}

			}
			else {
				fprintf(stderr, "Port entry is flawed\n");
				return 0;
			}
			break;
		case 'd':
			debug_mode = 1; //1 true, 0 false
			printf("Running in debug mode.\n");
			break;
		case '?':
			//TODO: print help here
			printf("Invalid argument...\n");
			return 0;
		}
	}
	return 1; //success
}

int setcliopts(struct clientopts *cliopt, struct sockaddr_in *consockinfo) {
	//TODO: make sanity checks, return error if addr or port is not entered!
	struct addrinfo **tempaddr = 0; //getaddrinfo() will fill this with possible inet addrs
	struct addrinfo hints; //we'll adjust it to filter getaddrinfo() results.
	int ret;

	//check if we have the port and address info ready
	if(cliopt->connectPortInt == 0) {
		fprintf(stderr, "You need to enter port number with -p.\n");
		return 0;
	}
	else if(cliopt->connectAddrString[0] == 0) {
		fprintf(stderr, "You need to enter an address with -a.\n");
		return 0;
	}

	consockinfo->sin_family = AF_INET;
	consockinfo->sin_port = htons((short) cliopt->connectPortInt); //cast and set to network endian the port we get from cmdline
	tempaddr = malloc(sizeof(struct addrinfo));
	memset(&hints, 0, sizeof(struct addrinfo));
	hints.ai_family = AF_INET;			//we are only interested in ipv4 addresses
	hints.ai_socktype = SOCK_STREAM;    //we are only interested in tcp connections
	hints.ai_flags = AI_ADDRCONFIG;		//we are only interested in real network (no localhost for example)

	if(debug_mode) printf("running the complex func getaddrinfo()\n");
	ret = getaddrinfo(cliopt->connectAddrString, 0, &hints, tempaddr);

	if(ret) fprintf(stderr, "we have an error %s\n", gai_strerror(ret));

	struct addrinfo *looplink = *tempaddr;
	while(looplink != NULL) { //loop through and print all addresses if we have more than one.
		/*
		 * cast is confusing. It goes like this:
		 * The list elements allocated by getaddrinfo are not sockaddr, they are sockaddr_in.
		 * We fix this with first cast.
		 * in_addr_t is an 32 bit IP addr, we want to think of it as an unsigned long.
		 * So we copy the addr to a new variable
		 * Finally we take the address of new variable and cast it as an unsigned char string
		 * to access the integer inside byte by byte.
		 * And yes, we could have done it one line too.
		 */
		unsigned long ipaddr = ((unsigned long) ((struct sockaddr_in *) (looplink)->ai_addr)->sin_addr.s_addr);
		unsigned char *ipstr = (unsigned char *)&ipaddr;
		if(debug_mode) printf("Got addr %u.%u.%u.%u after name resolution.\n", *ipstr, ipstr[1], ipstr[2], ipstr[3]);
		looplink = looplink->ai_next;
	}
	if(debug_mode) printf("Using the first address. \n");
	//a simpler cast. As I stated above we are actually given sockaddr_in structs, not sockaddrs
	consockinfo->sin_addr.s_addr = ((struct sockaddr_in *) (*tempaddr)->ai_addr)->sin_addr.s_addr; //take the first resolved address
	//TODO: do we need to call htonl? does getaddrinfo give us adjusted addresses? man page has no info, guess we need to experiment

	return 1;
}

int getWholeLog(int sd, struct messagelog *climsglog) {
	//read server log in MSG_LEN chunks
	char readbuf[MSG_LEN];
	int readFromServer = 0;
	memset(readbuf, 0 , MSG_LEN);
	/*while((readFromServer = recv(sd, readbuf, MSG_LEN, )) > 0) {
		printf("looping through data sent by server! %d\n", readFromServer);
		printf("%s\n", readbuf);
		writeMessageLog(climsglog, readbuf, readFromServer);
	}*/
	readFromServer = read(sd, readbuf, MSG_LEN);
	writeMessageLog(climsglog, readbuf, readFromServer);
}

//same as getWholeLog (but blocking) until we implement signals
int readMessage(int sd, struct messagelog *climsglog) {
	//read server log in MSG_LEN chunks
	char readbuf[MSG_LEN];
	int readFromServer = 0;
	memset(readbuf, 0 , MSG_LEN);
	int wrote; //for debug
	//this piece of code has problems
	//these loops are commented out but this particular one is also broken
	//TODO: rewrite the loop later.
	/*while((readFromServer = read(sd, readbuf, MSG_LEN)) > 0) {
		printf("looping through data sent by server! %d\n", readFromServer);
		printf("%s\n", readbuf);
		writeMessageLog(climsglog, readbuf, readFromServer);
	}*/
	if((readFromServer = read(sd, readbuf, MSG_LEN)) > 0) {
		if(debug_mode) printf("server sent this: %s\nmsglen: %d\n", readbuf, readFromServer);
		pthread_mutex_lock(&climsglog_mutex);
		wrote = writeMessageLog(climsglog, readbuf, readFromServer);
		pthread_mutex_unlock(&climsglog_mutex);
		if(debug_mode) printf("%d bytes written to log\n", wrote);
	}
	else {
		if(readFromServer == -1) {
			if(errno == ETIMEDOUT) printf("Connection to server timed out.\n");
			else perror("Server connection failed");
		}
		else printf("Server disconnected.\n"); //perror returns Success if server shuts down the connection which is a little surprising.
		return -1;
	}
}


int sendMessageToServer(int sd, char *climsg) {
	int sent, remaining = strlen(climsg);
	char *index = climsg;
	if(debug_mode) printf("sending message %d bytes.\n", remaining);
	while((sent=write(sd, index, remaining)) > 0) {
		if(debug_mode) printf("inside sendMessageToServer loop\n");
		remaining -= sent;
		index += sent;
		if(remaining<=0) break;
		if(debug_mode) printf("last sent: %d remaining: %d\n", sent, remaining);
	}
	//TODO: we need to change this somehow. we can't understand if write() fails.
}

void *sendThread(void *arg) {
	int *sd_ptr = arg;
	int sd = *sd_ptr; //I can't compile with one line casting
	char *climsg = malloc(MSG_LEN); //I hate both C and scanf for all this array stuff
	while(1) {
		memset(climsg, 0, MSG_LEN);
		printf("send msg max %d chars long: \n", MSG_LEN);
		scanf("%s", climsg);
		pthread_mutex_lock(&climsglog_mutex);
			sendMessageToServer(sd, climsg);
		pthread_mutex_unlock(&climsglog_mutex);
	}
	free(climsg);
}

void *recvThread(void *arg) {
	int *sd_ptr = arg;
	int sd = *sd_ptr; //I can't compile with one line casting

	while(1) {
		//check if server has anything to send us?
		if(debug_mode) printf("waiting for server messages\n");
		//readmessage takes care of locking, same should be arranged for sendMessageToServer
		if(readMessage(sd, &climsglog)==-1) {
			if(debug_mode) printf("receive failure, canceling send thread to quit.\n");
			pthread_cancel(send_thread); //we can wait for thread cancel by polling pthread_join()!
			return 0;
		}
		//print out what server has sent.
		char newmsg[MSG_LOG_LEN];
		memset(newmsg, 0 , MSG_LOG_LEN);
		pthread_mutex_lock(&climsglog_mutex);
		readDelta(&climsglog, newmsg, MSG_LOG_LEN, recvDelta);
		recvDelta = climsglog.end;
		pthread_mutex_unlock(&climsglog_mutex);
		//just in case we filled the whole buffer and we don't have a \0 at the end
		newmsg[MSG_LOG_LEN-1] = '\0';
		printf("new message: %s\n", newmsg);
	}
}

#endif /* CLIENT_H_ */
