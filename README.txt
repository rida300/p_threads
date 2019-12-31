Sources:
 
https://stackoverflow.com/questions/21405204/multithread-server-client-implementation-in-c
https://www.geeksforgeeks.org/g-fact-70/
https://randu.org/tutorials/threads/
https://www.cs.cmu.edu/afs/cs/academic/class/15492-f07/www/pthreads.html
https://www.tutorialspoint.com/how-do-i-convert-a-char-to-an-int-in-c-and-cplusplus
https://github.com/nikhilroxtomar/Chatroom-in-C
https://gist.github.com/Abhey/47e09377a527acfc2480dbc5515df872
https://dzone.com/articles/parallel-tcpip-socket-server-with-multi-threading
https://gist.github.com/oleksiiBobko/43d33b3c25c03bcc9b2b
 
To compile, run 'make' as there is a Makefile
To run directory server, ./directoryServer &
To run chatServer, ./server "topic" 7677 129.130.10.43 where 7677 is the port and 129.130.10.43 is the ip (i.e: ./server <topic> <port> <ip>)
To run the client, ./client
To quit the server, CTRL + C in the chatServer. This will remove the server from the directory server and also quit all the clients that are connected to this server.
 
Overview:
p_thread_mutex is used to lock the linked list storing chatServers(in directoryServer) and clients(in chatServer).
Max client name can be 100 characters
Max topic name can be 100 characters
Security cautions:
fgets will only receive a specified number of character, then stop reading.
strncmp and strncpy is used as it is more secure.
 
The directory server will not connect more than 10 chat servers as stated in the project description. It has a while loop that runs continuously and accepts a client or chatServer and spawns off a thread to process the added connection. Once the method called from the worker thread returns, the thread is detached. It has a swicth case in the worker method which will decide what to do if it is a server and a client.getsockopt() has been used in a few places to ensure that the connection is still alive. The 3 cases are used for
1. the addition of a server, the topic is checked to ensure deduping, and creating an 'entity' to store in the linked list
2. the addition of a client, the client is sent a list of chat rooms. Every line written is appended with a character: 'G' if more lines are going to be written and 'E' if it is the end of the chat rooms list. This way, the client knows when to stop reading the chat rooms.
3. the removal of a server, the server will be removed from the linked list. To confirm this, you can add a new server with the same topic as the one that was removed, it will pass the duplication test and successfully add.
 
Chat server:
It first registers with the directory server using a separate method which returns the port and assigns the ip address to a global variable. In the main, the server then connects to these values and constantly (in a while loop) listens to accept a new client. Once a client is received, a new thread is spawned off. The method called by the worker thread first reads the neme of the client, ensures that this isnt a duplicate name, if it is, it sends a '0' to the client indicating that it needs to enter a new name. If an acceptable name has been received, the server will write a '1' to the client so it stops asking for a name. Then it sends a msg to the client indicating that they have joined the chat. The first user will receive the msg stated in the project description. It then listens for msgs from the client so that they can be broadcasted. 
To remove the server, CTRL+c SIGINT signal is used. It will connect the server with directory server again and pass in the topic of the server that is  wuitting. The directory server will remove this node from the linked list. The chatServer will then send a msg to all of its clients and cause them to exit. The msg it passes is ':' which is compared on the client side and once received, the client exits. 
 
Client:
It connects to the directory server, receives a list of chatrooms with topic and ports, the client will select a port, type it in to connect and then they will be prompted to enter their name. If the name already exists in this chatroom, the clinet will have to enter a new name. The main creates a two threads, 1. for reading standard in
2. for receiving msgs from the server
CTR+c will quit the client
 