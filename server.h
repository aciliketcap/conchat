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

/*
 * todo list:
 *
 * * better control for received/sent message lengths (and uncommenting send receive loops)
 * * client identification. at least a login name.
 * * multiple chat rooms by using multiple msglog structs.
 * * some data structure to dynamically allocate and manage threads
 * * we can implement thread pools if we can manage multiple threads correctly
 *
 * notes:
 *
 *  * server does not support international domain names
 *  * since we have no mechanism to manage threads old thread numbers are not reused
 *
 */
#ifndef SERVER_H_
#define SERVER_H_

#include <unistd.h> //for getopt()
#include <stdlib.h> //for malloc()
#include <string.h> //for strncpy(), memset(), strlen()
#include <stdio.h>
#include <errno.h> //for perror

#include <netdb.h> //for getaddrinfo() and struct addrinfo ie. name resolution

#include <sys/socket.h> //socket structs and functions
#include <sys/types.h> //socket types and address families
#include <netinet/in.h> //some systems use this instead of arpa/inet.h
#include <arpa/inet.h> //IP related structs and functions

#include <pthread.h>


#include "messagelog.h"

#define MAX_THREADS 255
#define MAX_CONNECTIONS 1023

#define MAX_ADDR_LEN 256 //maximum address string length
#define MSG_LEN 256		//can be longer as long as it is less than MSG_LOG_LEN (it can be longer than MSG_LOG_LEN but that doesn't make sense)
#define MSG_LOG_LEN 1024  //TODO: for debug, should be longer

extern int debug_mode;
extern struct messagelog srvmsglog; //global log for all the threads
extern char *sendDelta; //unique to each thread, points to the place right after the last sent bytes. Like some sort of send queue for the thread.

extern pthread_mutex_t srvmsglog_mutex;
extern pthread_mutex_t readytosend_mutex;
extern pthread_cond_t readytosend_cond;
extern pthread_mutex_t csd_transfer_mutex;
extern int cur_thread_count;

extern pthread_t recv_thread[MAX_CONNECTIONS];
extern pthread_t send_thread[MAX_CONNECTIONS];

struct serveropts {
	int listenPortInt;	//the listen port taken directly from input. We'll convert this to unsigned short before using.
	char threadpool; 	//bool //use thread pool of size maxThreads.
	int maxConnections; //max number of connections to accept.
	int maxThreads;     //max number of dynamically created threads
	char addrString[MAX_ADDR_LEN]; //the address we will be listening to. If empty we'll use INADDR_ANY which listens to all IPs on host.
};

int getsrvopts(int argc, char **argv, struct serveropts *srvopt) {
	int c;
	while ((c = getopt(argc, argv, "a:p:t:l:Td")) != -1) {
		switch(c) {
		case 'a':
			if(!optarg) {
				printf("You need to enter an address!\n");
				return 0;
			}
			strncpy(srvopt->addrString, optarg, MAX_ADDR_LEN);
			if(srvopt->addrString[MAX_ADDR_LEN - 1] != '\0') { //which means address string overflowed
				fprintf(stderr, "Address is too long!\n");
				return 0;
			}
			else printf("Listen address is set to: %s\n", srvopt->addrString);
			break;
		case 'p':
			if(!optarg) {
				printf("You need to enter port number!\n");
				return 0;
			}
			srvopt->listenPortInt = 0;
			if(srvopt->listenPortInt = atoi(optarg)) { //convert port num to int first
				if(srvopt->listenPortInt < 0 || srvopt->listenPortInt > 65535) {
					fprintf(stderr, "Listen port should be between 0 and 65535.\n");
					return 0;
				}
				else {
					printf("Listen port number set to %d.\n", srvopt->listenPortInt);
				}
			}
			else {
				fprintf(stderr, "Port entry is flawed\n");
				return 0;
			}
			break;
		case 't':
				if(!optarg) {
					printf("You need to enter a thread count after option -t\n");
					return 0;
				}
				//convert thread limit to int.
				srvopt->maxThreads = 0;
				if(srvopt->maxThreads = atoi(optarg)) {
					if(srvopt->maxThreads < 0 || srvopt->maxThreads > MAX_THREADS) {
						fprintf(stderr, "Thread count should be between 0 and %d.\n", MAX_THREADS);
						return 0;
					}
					else {
						printf("Thread count set to %d.\n", srvopt->maxThreads);
					}
				}
				else {
					fprintf(stderr, "Thread count entry is flawed.\n");
					return 0;
				}
				break;
		case 'l':
				if(!optarg) {
					printf("You need to enter a connection count after option -l\n");
					return 0;
				}
				//convert thread limit to int.
				srvopt->maxConnections = 0;
				if(srvopt->maxConnections = atoi(optarg)) {
					if(srvopt->maxConnections < 0 || srvopt->maxConnections > MAX_CONNECTIONS) {
						fprintf(stderr, "Connection count should be between 0 and %d.\n", MAX_CONNECTIONS);
						return 0;
					}
					else {
						printf("Maximum number of accepted connections is set to %d.\n", srvopt->maxConnections);
					}
				}
				else {
					fprintf(stderr, "Connection count entry is flawed.\n");
					return 0;
				}
				break;
		case 'T':
			srvopt->threadpool = 1; //1 true, 0 false
			printf("Running with a constant thread pool instead of dynamically creating threads.\n");
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

int setsrvopts(struct serveropts *srvopt, struct sockaddr_in *srvsockinfo) {
	//TODO: special address and special interface cases are not handled at the moment.

	struct addrinfo **tempaddr = 0; //an empty array of addr structs which is processed by getaddrinfo() function.
	struct addrinfo hints; //hints to accept only ipv4 addresses with tcp available.
	int ret;

	srvsockinfo->sin_family = AF_INET; //this is an internet socket
	if(srvopt->listenPortInt < 1024) printf("warning: listening to port numbers smaller than 1024 requires root priviliges.\n");
	srvsockinfo->sin_port = htons((short) srvopt->listenPortInt); //change port number to short int in network byte order.

	//if specific address is given find and use it, else just use INADDR_ANY wildcard.
	if(srvopt->addrString[0] == 0) {
		srvsockinfo->sin_addr.s_addr = htonl(INADDR_ANY);
		/*
		 * using wildcard INADDR_ANY let's our socket to receive any packets came to our host.
		 * Kernel manages dest addresses of packages we send so that each connection
		 * gets packets from us with src IP equal to the one they used to connect to host.
		 */
	}
	else { //listening to the specified address only
		tempaddr = malloc(sizeof(struct addrinfo));
		memset(&hints, 0, sizeof(struct addrinfo));
		hints.ai_family = AF_INET;
		hints.ai_socktype = SOCK_STREAM;
		hints.ai_flags = 0;
		//hints.ai_flags = AI_NUMERICHOST; //AI_NUMERICHOST means, we only accept ip numbers, not the hostnames.
		//hints.ai_flags = AI_ADDRCONFIG; //And AI_ADDRCONFIG means the address should be a valid ipv4 addr. localhost is not accepted.
		if(debug_mode) printf("Resolving address with getaddrinfo()\n");
		ret = getaddrinfo(srvopt->addrString, 0, &hints, tempaddr);
		if(ret) {
			printf("Unable to resolve address: %s\n", gai_strerror(ret));
			return 0;

		}
		struct addrinfo *looplink = *tempaddr;
		while(looplink != NULL) {
			//TODO: make it one line!
			unsigned long ipaddr = ((struct sockaddr_in *) (looplink)->ai_addr)->sin_addr.s_addr;
			unsigned long *ipptr = &ipaddr;
			unsigned char *ipstr = (unsigned char *) ipptr;
			if(debug_mode) printf("resolved address is %u.%u.%u.%u\n", *ipstr, ipstr[1], ipstr[2], ipstr[3]);
			looplink = looplink->ai_next;
		}
		//TODO: check the resolved address is on the host, may be bind() can help.
	}
	return 1; //success
}

//TODO: This should lock the msglog struct for the sake of concurrency
int sendWholeLog(int csd, struct messagelog *msglog) {
	char logsendbuf[MSG_LOG_LEN];
	int sentlog;
	memset(logsendbuf, 0, MSG_LOG_LEN);
	pthread_mutex_lock(&srvmsglog_mutex);
		if(debug_mode) printf("locked srvmsglog mutex in send thread to send whole log\n");
		readMessageLog(msglog, logsendbuf, MSG_LOG_LEN);
	pthread_mutex_unlock(&srvmsglog_mutex);
	if(debug_mode) printf("unlocked srvmsglog mutex in send thread to send whole log\n");
	logsendbuf[MSG_LOG_LEN-1] = '\0'; //precaution for array overflow
	//just another cast error from compiler, it says char* and char[] are two different things and cannot be used interchangeably
	char *logsendbuf2 = logsendbuf;
	if(debug_mode) printf("Sending whole log to newly connected client: %s\n", logsendbuf2);
	while(strlen(logsendbuf2)>0 && (sentlog = write(csd, logsendbuf2, strlen(logsendbuf2))) > 0){
		if(debug_mode) printf("sent %d bytes\n", sentlog);
		logsendbuf2 += sentlog;
	}
	if(debug_mode) printf("FINAL: %d bytes long message log sent!\n",logsendbuf2-logsendbuf);
	return logsendbuf2 - logsendbuf;
}

//TODO: This should lock the msglog struct for the sake of concurrency
int receiveClientMessage(int csd, struct messagelog *msglog) {
	char *message = malloc(MSG_LEN);
	memset(message, 0, MSG_LEN);

	int recvd, remaining = MSG_LEN;
	char *index = message;

	if(debug_mode) printf("waiting for client message on csd: %d\n", csd);
	/*while((recvd = read(csd, index, remaining)) > 0) {
		printf("looping through the data client sent.\n");
		remaining -= recvd;
		index += recvd;
		if(remaining<=0) break;
	}*/
	if((recvd = read(csd, index, remaining)) <= 0) {
		if(recvd == -1) {
			if(errno == ETIMEDOUT) printf("Connection to client timed out.\n");
			else perror("Client connection failed");
		}
		else printf("Client disconnected.\n"); //perror returns Success if client shuts down the connection which is a little surprising.
		return -1;
	}
	if(message[MSG_LEN]!='\0') message[MSG_LEN]='\0'; //precaution to prevent array overflow
	if(debug_mode) printf("we have msg: %s\nmsglen: %d\n", message, recvd);
	pthread_mutex_lock(&srvmsglog_mutex);
		if(debug_mode) printf("locked srvmsglog mutex in receive thread\n");
		writeMessageLog(msglog, message, recvd);
		if(debug_mode) printf("received message written to srvmsglog\n");
	pthread_mutex_unlock(&srvmsglog_mutex);
	if(debug_mode) printf("unlocked srvmsglog mutex in receive thread\n");
	free(message);
}

/*
 * this function is needed because if thread is terminated while waiting on cond mutex
 * mutex is locked as if the cond_wait function returned. Pretty troublesome but
 * man page says it is for a good reason.
 */
void sendThreadCleanup(void* arg) {
	int *wait_for_cond = (int *) arg;
	if(*wait_for_cond) pthread_mutex_unlock(&readytosend_mutex); //arg is a pointer to waiting_for_cond
	//condition mutex is locked if thread receives cancellation request while it is blocked by pthread_cond_wait()
	printf("readytosend mutex unlocked after cancel request\n");
}

void* sendThread(void *thread_csd) {
	int sent = 0;
	int msglen = 0;
	char newmsga[MSG_LEN];
	char *newmsg = newmsga; //so that compiler will shut up about incrementing newmsg
	int waiting_for_cond = 0; //this is needed to determine if we need to unlock readytosend mutex on receiving pthread_cancel()

	int cur_thread_num = cur_thread_count;
	int *csd_ptr = thread_csd;
	int csd = *csd_ptr; //casting in one step causes compiler errors :(
	if(debug_mode) printf("csd %d and thread no %d taken successfully\n", csd, cur_thread_num);
	//TODO: may be we should pass these with function args, locking still needed though
	pthread_mutex_unlock(&csd_transfer_mutex); //ok, we took csd before it changed
	pthread_cleanup_push(&sendThreadCleanup, (void *) &waiting_for_cond);
	if(debug_mode) printf("sending the server log\n");
	int logsent = sendWholeLog(csd, &srvmsglog); //sendWholeLog function takes care of locking
	if(debug_mode) printf("sendWholeLog() function returned with %d in thread %d\n", logsent, cur_thread_num);
	//TODO: locking right after sendWholeLog() seems like too many locks, find some way to adjust delta in sendWholeLog()
	//TODO: may be we can pass sendDelta by reference
	pthread_mutex_lock(&srvmsglog_mutex);
		if(debug_mode) printf("locked srvmsglog mutex in send thread %d to adjust delta\n", cur_thread_num);
		char *sendDelta = srvmsglog.end; //adjust delta for the thread
	pthread_mutex_unlock(&srvmsglog_mutex);
	if(debug_mode) printf("unlocked srvmsglog mutex in send thread %d after adjusting delta\n", cur_thread_num);
	if(debug_mode) printf("delta updated to %x by thread %d\n",sendDelta, cur_thread_num);

	//the send loop
	while(1) {
		if(debug_mode) printf("waiting for new messages to send, csd is %d thread num is %d\n", csd, cur_thread_num);
		pthread_mutex_lock(&readytosend_mutex);
		waiting_for_cond = 1;
		if(debug_mode) printf("locked readytosend mutex in send thread %d to setup cond_wait\n", cur_thread_num);
		pthread_cond_wait(&readytosend_cond, &readytosend_mutex); //automatically locks readytosend_mutex just before return!
		//what happens when the thread is canceled in pthread_cond_wait is defined very well on man page. (man 3 pthread_cond_wait)
		pthread_mutex_unlock(&readytosend_mutex);
		waiting_for_cond = 0;
		if(debug_mode) printf("unlocked readytosend mutex in send thread %d after cond signal taken\n", cur_thread_num);
		if(debug_mode) printf("new message ready signal taken, csd is %d thread num is %d\n", csd, cur_thread_num);
		pthread_mutex_lock(&srvmsglog_mutex);
			if(debug_mode) printf("locked srvmsglog mutex in send thread %d\n", cur_thread_num);
			msglen = readDelta(&srvmsglog, newmsg, 0, sendDelta);
			if(debug_mode) printf("message to send is: %s\nmessage length: %d thread: %d\n", newmsg, msglen, cur_thread_num);
		pthread_mutex_unlock(&srvmsglog_mutex);
		if(debug_mode) printf("unlocked srvmsglog mutex in send thread\n", cur_thread_num);

		if(debug_mode) printf("thread %d's csd for sending is %d\n", cur_thread_num, csd);
		if(msglen > 0 && (sent = write(csd, newmsg, msglen)) > 0){
			if(debug_mode) printf("%d long message sent via thread %d.\n", sent, cur_thread_num);
			pthread_mutex_lock(&srvmsglog_mutex);
				if(debug_mode) printf("locked srvmsglog mutex in send thread\n", cur_thread_num);
				sendDelta = srvmsglog.end; //TODO: fixing delta in readDelta function is more efficient
			pthread_mutex_unlock(&srvmsglog_mutex);
			if(debug_mode) printf("unlocked srvmsglog mutex in send thread\n", cur_thread_num);
		}
		else {
			if(sent == -1) {
				if(errno == ETIMEDOUT) printf("Connection to client timed out.\n");
				else perror("Client connection failed");
			}
			else printf("Client disconnected.\n"); //perror returns Success if client shuts down the connection which is a little surprising.
			//It is recv threads responsibility to stop both threads. If connection is really failed, recv thread will end itself.
			return;
		}
	}
	pthread_cleanup_pop(0); //guess what! cleanup push and pop have to be used together and in the same block. checkout the source or search "pthread_cleanup_pop macro broken"
}

void* recvThread(void *thread_csd) {

	int cur_thread_num = cur_thread_count;
	void *join_ret;
	int *csd_ptr = thread_csd;
	int csd = *csd_ptr; 	//casting in one step causes compiler errors :(
	pthread_mutex_unlock(&csd_transfer_mutex); //ok, we took csd before it changed

	while(1) {
		if(debug_mode) printf("waiting for client msg, csd is %d thread num is %d\n", csd, cur_thread_num);
		if(receiveClientMessage(csd, &srvmsglog) == -1) { //TODO: should be blocking but it does not block?
			printf("Client disconnected.\n");
			if(debug_mode) printf("Destroying send thread %d\n", cur_thread_num);
			pthread_cancel(send_thread[cur_thread_num]);
			pthread_join(send_thread[cur_thread_num], &join_ret);
			//just to make sure it is destroyed, check return value of pthread_join
			if(debug_mode) printf("Returning from recv thread %d\n", cur_thread_num);
			return;
		}
		//receiveClientMessage takes care of locking
		if(debug_mode) printf("new message recvd by thread %d, csd %d\n", cur_thread_num, csd);
		pthread_mutex_lock(&readytosend_mutex);
			if(debug_mode) printf("locked readytosend mutex in recv thread\n");
			pthread_cond_broadcast(&readytosend_cond);
			if(debug_mode) printf("Ready to send broadcasted to all send threads by thread %d\n", cur_thread_num);
		pthread_mutex_unlock(&readytosend_mutex);
		if(debug_mode) printf("unlocked readytosend mutex in recv thread\n");
	}
}



#endif /* SERVER_H_ */
