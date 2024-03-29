This is a threaded server and client implementation which sends and receives strings between clients. (It is not a real chat server.)

It was developed as a training project. I wanted to use pthreads and sockets. Code may use many small improvements and I wrote them down as TODO comments inside the code.

I also wrote a [blog post] (http://aciliketcap.blogspot.com/2012/12/a-threaded-server-implementation.html) on how it works in general. To summarize:

1. I made a queue class which takes C strings as input. Code turned out complicated but I guess it is more efficient than pushing chars.
2. Messages sent and received  are also null terminated strings. The problem is client uses scanf() to take input from client user. So you can't use white space. That can be fixed by using a different input method but I didn't bothered to change it.
3. One last thing about C strings is they give way to security flaws. I didn't paid special attention to that and I suspect some parts are open to stack overflow exploits using special packets.
4. I used threaded approach to learn about threads. Polling/event handling is another possibility. The threads however are using blocked I/O and won't eat CPU cycles when server is idle.
5. Threads are not managed. It definitely needs to create threads dynamically and keep a track of them. A linked list might do the job I think.
6. Again it is not a real chat server. It just takes messages from clients, notes them down and sends them to all other clients. Messages are just C strings, there is no protocol like IRC or something.

You can find more things that needs to be implemented in server.h file's beginning.

I learned so much while coding this. About threads, TCP, sockets, server design. However the most important thing I learnt is this: If I ever need to write a server for some real project I will definitely use C++. Anything above (I mean higher level than) socket programming becomes too complicated without classes I think.
