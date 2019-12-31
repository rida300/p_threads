#
# Makefile for Example
#
LIBS	=
CFLAGS	= -g -pedantic

all:	tcp 

#
# Internet stream version (TCP protocol).
#

tcp:	chatServer5 chatClient5 directoryServer5
chatClient5.o chatServer5.o directoryServer5.o: inet.h

chatServer5:	chatServer5.o 
	#gcc -Wall -o server chatServer2.c -lssl -lcrypto
	gcc -Wall -o server chatServer5.c -lpthread


#openssl req -x509 -nodes -days 365 -newkey rsa:1024 -keyout certChatServer.pem -out certChatServer.pem
# openssl pkey -in certChatServer.pem to retrieve the private key of the certificate



chatClient5:	chatClient5.o 
	gcc -Wall -o client chatClient5.c -lpthread

directoryServer5:	directoryServer5.o 
	gcc -Wall -o directoryServer directoryServer5.c -lpthread

#
# Clean up the mess we made
#
clean:
	rm *.o \
	server4 client4
 
