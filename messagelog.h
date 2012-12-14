/*
 * messagelog.h
 * Copyright 2012 Sinan Akpolat
 *
 * Message log struct and associating functions work like a string queue/buffer.
 * However you don't push strings char by char, you push them as a whole.
 * While this complicates the source code, strings are copied as a whole (or
 * at most two parts) by memcpy, which I think improves performance.
 *
 * Secondly it doesn't stop when it is full, it just overwrites old information.
 * It doesn't overwrite the string you are pushing though, it just copies a substring
 * as long as queue size. In other words if you try to push a string longer than the
 * max queue size, it only pushes the part that fits.
 *
 * This file is distributed under GNU GPLv3, see LICENSE file.
 * If you haven't received a file named LICENSE see <http://www.gnu.org/licences>
 *
 * Message log queue/buffer is distributed WITHOUT ANY WARRANTY;
 * without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE
 */

#ifndef MESSAGELOG_H_
#define MESSAGELOG_H_

#include <stdlib.h> //for malloc()
#include <string.h> //for strncpy()
#include <stdio.h>


/*
 *  Alright, here is how it works:
 *  normal case:
 *    |------------|=============|------------|
 *  data         begin          end        dataend
 *  wrapped around case:
 *    |========|-------|======================|
 *  data      end    begin                 dataend
 *  max queue size = dataend - data
 *  normal case:
 *  size = end - begin
 *  wrapped around case:
 *  size = (dataend - begin) + (end - data) = end - begin + max queue size
 */
struct messagelog { //it is essentially a queue
	char *data;
	char *begin;	//beginning of queue
	char *end;		//end of queue
	char *dataend;  //dataend - data = size
};

void initMessageLog(struct messagelog *msglog, int size) {
	//a char is lost to distinguish queue full and queue empty situations
	msglog->data = malloc(size+1);
	msglog->begin = msglog->data;
	msglog->end = msglog->data;
	msglog->dataend = msglog->data + size+1;
}
void freeMessageLog(struct messagelog *msglog) {
	free(msglog->data);
}

//more commonly known as push()
int writeMessageLog(struct messagelog *msglog, char *writebuf, int len) {
	int rem = 0; //remaining bytes to copy
	char *bufindex = writebuf;
	int ret = 0; //return number of bytes copies

	if(len >= msglog->dataend - msglog->data) rem = msglog->dataend - msglog->data -1; //this is not likely as long as max queue len > max msg len
	else rem = len;
	ret = rem; //all cases copy until rem = 0 so it is safe to assume we will copy rem bytes

	if(msglog->begin <= msglog->end) { //normal queue
		if((msglog->dataend-msglog->end) < len) { //write till end, then begin from start and write again
			memcpy(msglog->end, bufindex, msglog->dataend-msglog->end);
			bufindex += msglog->dataend-msglog->end;
			rem -= msglog->dataend-msglog->end;
			memcpy(msglog->data, bufindex, rem);
			if(msglog->data+rem > msglog->begin) {
				msglog->end = msglog->data+rem;
				msglog->begin = msglog->end+1;
			}
			else {
				msglog->end = msglog->data+rem;
			}
		}
		else { //just write sequentially. we also know begin is to the left of end so the easiest case I guess
			memcpy(msglog->end, bufindex, rem);
			msglog->end = msglog->end+rem;
		}
	}
	else { // the queue is wrapped around. end is to the left of the beginning.
		if(msglog->end+rem >= msglog->begin) { //if we need to overwrite older messages
			if(msglog->end+rem > msglog->dataend) { //do we need to start again from msglog->data after passing start?
				//copy till the end of data (dataend)
				memcpy(msglog->end, bufindex, msglog->dataend-msglog->end);
				bufindex += msglog->dataend-msglog->end;
				rem -= msglog->dataend - msglog->end;
				//copy the rest starting from the beginning of data
				memcpy(msglog->data, bufindex, rem);
				if(msglog->data+rem > msglog->end) {
					msglog->end = msglog->data+rem;
					msglog->begin = msglog->end+1;
				}
				else {
					msglog->end = msglog->data+rem;
					msglog->begin = msglog->end+1;
				}
			}
			else {
				memcpy(msglog->end, bufindex, rem);
				msglog->end = msglog->end + rem;
				msglog->begin = msglog->end+1;
			}
		}
		else { //relatively easier
			memcpy(msglog->end, bufindex, rem);
			msglog->end = msglog->end+rem;
		}
	}
	return rem;
}

//removes len long string from message log aka. pop()
//if len = 0 copies whole queue and empties it

int removeMessageLog(struct messagelog *msglog, char *readbuf, int len) {
	int rem = len;  //remaining bytes to copy
	char *bufindex = readbuf;
	int ret = 0; //return number of bytes copies

	if(msglog->begin == msglog->end) {//if queue is empty return right away.
		readbuf[0] = '\0';
		return 0;
	}

	if(msglog->begin <= msglog->end) {	//normal queue, easy case
		//fix len if it is larger than used size.
		if(rem==0) rem = msglog->end - msglog->begin -1; //print from start to end
		else if(rem > msglog->end - msglog->begin -1) rem = msglog->end - msglog->begin;
		ret = rem; //all cases copy until rem = 0 so it is safe to assume we will copy rem bytes

		if(msglog->end - msglog->begin < rem) rem = msglog->end - msglog->begin;
		memcpy(bufindex, msglog->begin, rem);
		msglog->begin += rem; //we have emptied the queue if rem = msglog->end - msglog->begin
	}
	else { //queue is wrapped around itself
		//fix len if it is larger than used size.
		if(rem==0) rem = (msglog->dataend-msglog->begin) + (msglog->end-msglog->data) -1;
		else if(rem > (msglog->dataend-msglog->begin) + (msglog->end-msglog->data)-1) rem = (msglog->dataend-msglog->begin) + (msglog->end-msglog->data);
		ret = rem; //all cases copy until rem = 0 so it is safe to assume we will copy rem bytes

		if(msglog->begin + rem < msglog->dataend) { //easier case
			memcpy(bufindex, msglog->begin, rem);
			msglog->begin += rem;
		}
		else { //we need to copy two times
			memcpy(bufindex, msglog->begin, msglog->dataend-msglog->begin);
			rem -= msglog->dataend-msglog->begin;
			readbuf += msglog->dataend-msglog->begin;
			//don't copy past msglog->end
			if(msglog->end-msglog->data < rem) rem = msglog->end - msglog->data;
			memcpy(bufindex, msglog->data, rem);
			msglog->begin = msglog->data + rem;
		}
	}
	return ret;
}

//like removeMessageLog but just copies, no change on the msglog
int readMessageLog(struct messagelog *msglog, char *readbuf, int len) {
	int rem = len;  //remaining bytes to copy
	char *bufindex = readbuf;
	int ret = 0; //return number of bytes copies

	if(msglog->begin == msglog->end) { //if queue is empty return right away.
		readbuf[0] = '\0';
		return 0;
	}

	if(msglog->begin <= msglog->end) { //normal queue
		if(rem==0) {
			rem = msglog->end - msglog->begin; //print from start to end
			printf("rem = %d\n", rem);
		}
		else if(rem >= msglog->end - msglog->begin) rem = msglog->end - msglog->begin; //fix len if it is larger than used size.
		memcpy(bufindex, msglog->begin, rem);
		ret = rem;
	}
	else { //queue wrapped around
		//fix len if it is larger than used size.
		if(rem==0) rem = (msglog->dataend-msglog->begin) + (msglog->end-msglog->data);
		else if(rem >= (msglog->dataend-msglog->begin) + (msglog->end-msglog->data)) rem = (msglog->dataend-msglog->begin) + (msglog->end-msglog->data);
		ret = rem;

		if(rem > msglog->dataend - msglog->begin){
			memcpy(bufindex, msglog->begin, msglog->dataend - msglog->begin);
			rem -= msglog->dataend - msglog->begin;
			bufindex += msglog->dataend - msglog->begin;
			memcpy(bufindex, msglog->data, rem);
		}
		else {
			memcpy(bufindex, msglog->begin, rem);
		}
	}
	return rem;
}

//this will copy the last message pushed into the queue
//just like readMessageLog() above, but reads from msglog->delta instead of msglog->begin


/*int readDelta(struct messagelog *msglog, char *readbuf, int len, char *delta) {
	int rem = len;  //remaining bytes to copy
	char *bufindex = readbuf;
	int ret = 0; //return number of bytes copies

	if(msglog->delta == msglog->end) { //if no new messages then return
		readbuf[0] = '\0';
		return 0;
	}

	if(msglog->delta <= msglog->end) { //normal queue
		if(rem==0) rem = msglog->end - msglog->delta; //copy from delta to end
		else if(rem >= msglog->end - msglog->delta) rem = msglog->end - msglog->delta; //fix len if it is larger than used size.

		memcpy(bufindex, msglog->delta, rem);
		ret = rem;
	}
	else { //queue wrapped around
		//fix len if it is larger than used size.
		if(rem==0) rem = (msglog->dataend-msglog->delta) + (msglog->end-msglog->data);
		else if(rem >= (msglog->dataend-msglog->delta) + (msglog->end-msglog->data)) rem = (msglog->dataend-msglog->delta) + (msglog->end-msglog->data);
		ret = rem;

		if(rem > msglog->dataend - msglog->delta){
			memcpy(bufindex, msglog->delta, msglog->dataend - msglog->delta);
			rem -= msglog->dataend - msglog->delta;
			bufindex += msglog->dataend - msglog->delta;
			memcpy(bufindex, msglog->data, rem);
		}
		else {
			memcpy(bufindex, msglog->delta, rem);
		}
	}
	return rem;
}*/

//just like read delta but set delta = end after reading
//so it seems like there is no new messages

//this will copy the last entries after delta and set delta to it's new position
int readDelta(struct messagelog *msglog, char *readbuf, int len, char *delta) {
	int rem = len;  //remaining bytes to copy
	char *bufindex = readbuf;
	int ret = 0; //return number of bytes copies

	if(delta == msglog->end) { //if no new messages then return
		readbuf[0] = '\0';
		return 0;
	}

	if(delta <= msglog->end) { //normal queue
		if(rem==0) rem = msglog->end - delta; //copy from delta to end
		else if(rem >= msglog->end - delta) rem = msglog->end - delta; //fix len if it is larger than used size.

		memcpy(bufindex, delta, rem);
		ret = rem;
	}
	else { //queue wrapped around
		//fix len if it is larger than used size.
		if(rem==0) rem = (msglog->dataend-delta) + (msglog->end-msglog->data);
		else if(rem >= (msglog->dataend-delta) + (msglog->end-msglog->data)) rem = (msglog->dataend-delta) + (msglog->end-msglog->data);
		ret = rem;

		if(rem > msglog->dataend - delta){
			memcpy(bufindex, delta, msglog->dataend - delta);
			rem -= msglog->dataend - delta;
			bufindex += msglog->dataend - delta;
			memcpy(bufindex, msglog->data, rem);
		}
		else {
			memcpy(bufindex, delta, rem);
		}
	}
	delta = msglog->end;
	return rem;
}

#endif /* MESSAGELOG_H_ */
