/*
 * server.h
 * Copyright 2012 Sinan Akpolat
 *
 * A concurrent chat server
 *
 * This file is distributed under GNU GPLv3, see LICENSE file.
 * If you haven't received a file named LICENSE see <http://www.gnu.org/licences>
 *
 * This chat server is distributed WITHOUT ANY WARRANTY;
 * without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE
 */

#include "server.h"
#include "messagelog.h"
#include <string.h> //for memset

#include <sys/socket.h> //socket structs and functions
#include <sys/types.h> //socket types and address families
#include <netinet/in.h> //some systems use this instead of arpa/inet.h
#include <arpa/inet.h> //IP related structs and functions

#include <pthread.h>	//multi-threading

int debug_mode = 0; //1 true, 0 false

struct serveropts srvopt;
int sd = 0; //server's socket descriptor to accept new connections
struct sockaddr_in srvsockinfo; //internet socket info struct to hold our addr/port
struct sockaddr_in clisockinfo; //client's socket information which holds her addr/port
socklen_t clisocklen = sizeof(struct sockaddr_in); //size of connecting client's socket struct

struct messagelog srvmsglog; //we can use multiple dynamically allocated logs for multiple chat rooms
pthread_mutex_t srvmsglog_mutex = PTHREAD_MUTEX_INITIALIZER; //mutex to lock srvmsglog
pthread_mutex_t readytosend_mutex = PTHREAD_MUTEX_INITIALIZER; //mutex to use with readytosend condition
pthread_cond_t readytosend_cond = PTHREAD_COND_INITIALIZER;  //condition struct to broadcast all threads that new messages arrived.

//variables unique to each thread:
int csd = 0; //socket descriptor to the established connection with a client
char *sendDelta; //position in the srvmsglog after the last sent message
//srvmsglog->end - sendDelta = remaining bytes that should be sent to the client

pthread_mutex_t csd_transfer_mutex = PTHREAD_MUTEX_INITIALIZER; //mutex to make sure csd is transferred safely to the thread.

pthread_t recv_thread[MAX_CONNECTIONS]; //TODO: can we make this dynamic? like holding thread types in a queue
pthread_t send_thread[MAX_CONNECTIONS]; //TODO: can we make this dynamic? like holding thread types in a queue
int cur_thread_count = 0; //how many threads are occupied right now?
//TODO: we should definitely make this dynamic and use old abandoned thread types again.


int main(int argc, char** argv) {
	memset(&srvopt, 0, sizeof(struct serveropts));
	if(!getsrvopts(argc, argv, &srvopt)) {
		fprintf(stderr, "Something failed while processing command line arguments.\n");
		return 0;
	}
	//now verify and set the options
	if(!setsrvopts(&srvopt, &srvsockinfo)) {
		fprintf(stderr, "Unable to setup listen socket.\n");
	}
	//create the socket
	sd = socket(PF_INET, SOCK_STREAM, 0); //create a tcp/ip socket
	if(sd == -1) {
		perror("Unable to create a socket: ");
		return 0;
	}
	//adjusting SO_REUSEADDR let's us reuse addr/port couples without waiting for them to be released
	//doesn't happen all the time but OS can sometimes make us wait on free addresses/ports
	int yes = 1;
	setsockopt(sd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes)); //just reuse the addr even if it looks busy because of a previous connections/binding.
	//bind a specific addr/port
	if(bind(sd, (struct sockaddr *) &srvsockinfo, sizeof(struct sockaddr_in))) {
		perror("Unable to bind to address/port on the host: ");
		close(sd);
		return 0;
	}
	//listen to the addr/port for new TCP connections
	if(listen(sd,srvopt.maxConnections ? : MAX_CONNECTIONS)) {
		fprintf(stderr, "Unable to start listening on port %d:", srvopt.listenPortInt);
		perror("");
		close(sd);
		return 0;
	}

	//initialize shared message log
	initMessageLog(&srvmsglog, MSG_LOG_LEN);

	//TODO: log should start empty normally, remove this in production release
	//let's write something inside to test first connection
	char motd[64] = "Congratulations, you have successfully connected to the server.";
	//no need to lock we didn't create any threads yet.
	writeMessageLog(&srvmsglog, motd, 63); //don't take the ending \0

	/*
	 * TODO: If thread pool is on, we should create the pool here
	 */

	while(1) { //this while loop will assign connections to threads
		if(debug_mode) printf("waiting for connections...\n");
		csd = accept(sd, (struct sockaddr *) &clisockinfo, &clisocklen);
		if(csd == -1) { //accept() failed.
			if(debug_mode) perror("Cannot accept new connection: ");
		}
		else {
			if(debug_mode) printf("someone connected...\n");
			//TODO: may be we can use original thread no's instead of cur_thread_count
			cur_thread_count++;
			pthread_mutex_lock(&csd_transfer_mutex); //unlocked be thread after safely copying csd
			if(pthread_create(&(send_thread[cur_thread_count]), NULL, &sendThread, &csd)) {
				pthread_mutex_unlock(&csd_transfer_mutex);
				fprintf(stderr, "Unable to create new send thread for incoming connection: ");
				perror(""); //not sure if pthread_create sets errno...
				cur_thread_count--;
				continue;
			}
			pthread_mutex_lock(&csd_transfer_mutex); //unlocked be thread after safely copying csd
			if(pthread_create(&(recv_thread[cur_thread_count]), NULL, &recvThread, &csd)) {
				pthread_mutex_unlock(&csd_transfer_mutex);
				fprintf(stderr, "Unable to create new recv thread for incoming connection: ");
				perror(""); //not sure if pthread_create sets errno...
				pthread_cancel(send_thread[cur_thread_count]); //cancel send thread too as a roll back.
				cur_thread_count--;
				continue;
			}
		}
	}
	close(sd);
	return 0;
}


















