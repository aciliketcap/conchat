This is a threaded server and client implementation. (It is not a real chat server)

It was developed as a training project. I wanted to use pthreads and sockets.

It took too much time and I have so many things to say about how it works but I will write a blog post about this in my blog (and give a link here). To summarize:

1. I made a queue class which takes C strings as input. Code turned out complicated but I guess it is more efficient than pushing chars.
2. Messages sent and received also null terminated strings. The problem is client uses scanf() to take input from client user. So you can't use white space. That can be fixed by using a different input method but I didn't bothered to change it.
3. One last thing about C strings is they give way to security flaws. I didn't paid special attention to that and I suspect some parts are open to stack overflows using special packets.
4. I used threaded approach to learn about threads. Polling/event handling is another possibility. The threads however are using blocked I/O and won't eat CPU cycles when server is idle.
5. Threads are not managed. It definitely needs to create threads dynamically and keep track of them. A linked list might do the job I think.
6. Again it is not a real chat server. It just take messages from clients, notes them down and sends them to all other clients. Messages are just C strings, there is no protocol like IRC or something.

You can find more things that needs to be implemented in server.h file's beginning.

I learned so much while coding this. About threads, TCP, sockets, server design.However the most important thing I learnt is this: If I ever need to write a server for some real project I will definitely use C++. Anything above (I mean higher level than) socket programming becomes too complicated without classes I think.
