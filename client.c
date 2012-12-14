/*
 * client.c
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

#include "client.h"
#include "messagelog.h"

#include <string.h> //for memset

#include <sys/socket.h> //socket structs and functions
#include <sys/types.h> //socket types and address families
#include <netinet/in.h> //some systems use this instead of arpa/inet.h
#include <arpa/inet.h> //IP related structs and functions

#include <pthread.h>

int debug_mode = 0; //1 true, 0 false

struct clientopts cliopt;
int sd = 0; //our connection socket descriptor.
struct sockaddr_in consockinfo; //connection socket info for addr/port.
socklen_t consocklen = sizeof(struct sockaddr_in); //size of the addr/port struct we are using for connection.

struct messagelog climsglog;
char *recvDelta;

pthread_t send_thread;
pthread_t recv_thread;
void *dummy; //for thread return values, we don't care about return values.

int main(int argc, char** argv) {
	memset(&cliopt, 0, sizeof(struct clientopts));
	//get the options from command line.
	if(!getcliopts(argc, argv, &cliopt)) {
		fprintf(stderr, "something failed after getopt()\n");
		return 0;
	}
	//take the options and stuff them in a sockaddr_in
	//resolve any addresses on the way
	if(!setcliopts(&cliopt, &consockinfo)){
		fprintf(stderr, "something failed trying to setup connection socket.\n");
	}
	//create the socket
	sd = socket(PF_INET, SOCK_STREAM, 0); //create a tcp/ip socket
	if(sd == -1) {
		perror("unable to create a socket");
		return 0;
	}
	//TODO: I guess we don't need this in client code, it should use ephemeral ports on client side.
	int yes = 1;
	setsockopt(sd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes)); //just reuse the addr even if it looks busy because of a previous connections/binding.
	//now let's see if anyone answer on the other side.
	if(connect(sd, (struct sockaddr *)&consockinfo, consocklen)) {
		perror("Problem connecting to the server");
		close(sd);
		return 0;
	}

	if(debug_mode) printf("connected to server\ntrying to read the server log\n");

	//let's get what other people wrote to server
	initMessageLog(&climsglog, MSG_LOG_LEN);
	//when we connect server should start sending us the log
	getWholeLog(sd, &climsglog);
	//print out what server has sent.
	char wholelog[MSG_LOG_LEN];
	memset(wholelog, 0 , MSG_LOG_LEN);
	readMessageLog(&climsglog, wholelog, 0);
	recvDelta = climsglog.end;
	//just in case we filled the whole buffer and we don't have a \0 at the end
	wholelog[MSG_LOG_LEN-1] = '\0';
	printf("server has sent its message log: \n%s\n", wholelog);

	if(pthread_create(&send_thread, NULL, &sendThread, &sd)) {
		perror("Unable to create send thread: ");
		return 0;
	}
	if(pthread_create(&recv_thread, NULL, &recvThread, &sd)) {
		perror("Unable to create receive thread: ");
		return 0;
	}

	pthread_join(send_thread, dummy); //wait indefinitely till thread's job is over.
	pthread_join(recv_thread, dummy); //wait indefinitely till thread's job is over.

}
