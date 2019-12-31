#include <time.h>
#include <string.h>
#include "inet.h"
#include "sem.h"
#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <signal.h>
#include <malloc.h>  /*FOR MEMORY ALLOCATION */
#include <arpa/inet.h>  /*for using ascii to network bit*/
#include <netinet/in.h>        /* network to asii bit */
#include <resolv.h>  /*server to find out the runner's IP address*/ 
#include <pthread.h> //use p_thread library

#define DEBUG 1
#define	MAX 100
#define MAX_CLIENTS 10
#define BUFFER_SZ 2048

void sigintHandler();
int refuse_duplicates(char *);
void registerServer(char * , char *, char * );
void removeClients();
void * do_something(void * );
void *handle_client(void *);

static _Atomic unsigned int cli_count = 0;
static int uid = 10;

struct client* headptr = NULL;
pthread_mutex_t clients_mutex = PTHREAD_MUTEX_INITIALIZER;

char * globalTopic;
char * globalPort;
char * globalIp;
char buffer[MAX];
int portSelected;
int sockFdDirectory;
struct sockaddr_in  cliu_addr, servu_addr;
int sigResult=0;
int clients[100];

typedef struct client {
    struct sockaddr_in address;
    int uid;
    int sockfd;
    char name[MAX];
    struct client * next;
    struct client * prev;
};

/*adds client to the linked list, calls refuse)dups to ensure no duplicates. Asks the user to re-enter 
name if empty or duplicate*/

/*queries linked list to see if new client's name is already present*/
/*returns 1 if given name is empty, -1 if duplicate found*/
int refuse_duplicates(char * name)
{
    pthread_mutex_lock(&clients_mutex);
    if (headptr == NULL)
    {
        exit(-1);
    }
    pthread_mutex_unlock(&clients_mutex);
    if (strncmp(name, "", 100) == 0 || strncmp(name, "\0", 1) == 0 || strncmp(name, " ", 1) == 0 || name == NULL) {
     
        return -1;
    }
    pthread_mutex_lock(&clients_mutex);
    struct client * currentNode = headptr;
    while (currentNode->next != NULL)
    {
        
        if (strncmp(currentNode->next->name, name, strlen(name)) == 0 || strncmp(name, "", 1)==0)
        {
             pthread_mutex_unlock(&clients_mutex);
            return -1;
        }
        currentNode = currentNode->next;
    }
    pthread_mutex_unlock(&clients_mutex);
    return 0;

}
/*to re register server, sigint handler is used, it sends the indication to 
directory server to remove it from the list of available chat room*/
void sigintHandler()
{   
    int c;
    char * append = malloc(sizeof(char)*100);
	fprintf(stderr,"Goodbye! \n");
	/*removeClients();*/
    sprintf(append, "R,%s", globalTopic);
    registerToRemove(append);
    for(c=0; c<cli_count; c++)
    {
        int singleClient = clients[c];//sockfd
        int writtenQuit = write(singleClient, ":", sizeof(char));
    }
    //int writtenQuit = write(, append, strlen(append));
    sigResult = 2;
    close(sockFdDirectory);
    exit(0);
}

int main(int argc, char * argv[4])
{
    if(argc!= 4)
    {
        fprintf(stderr, "Please provide the topic and port number\n");
        printf("Usage: %s <'topic'> <port> <ip>\n", argv[0]);
        exit(1);
    }
    
    int             option = 1;
    pthread_attr_t  attr;
    struct          sockaddr_in  cli_addr, serv_addr;
    int             addResult;
    int	            clilen, childpid;
	char	        s[MAX];
	char	        request;
	pthread_t       tid;
    int             listenfd;
    int             acceptedfd;
    
    globalTopic = malloc(sizeof(char)*100);
    strncpy(globalTopic, argv[1], 100);
    globalPort = malloc(sizeof(char)*5);
    strncpy(globalPort, argv[2], 4);
    globalIp = malloc(sizeof(char)*14);
    strncpy(globalIp, argv[3], 15);
   
    registerServer(globalTopic, globalPort, globalIp);
    
    signal(SIGINT, &sigintHandler);
    //signal(SIGTSTP, &sigintHandler);
    /* Ignore pipe signals */
	//signal(SIGPIPE, SIG_IGN);

    struct client * head = (struct client *) malloc((sizeof(struct client)));
    strncpy(head->name, "\0", 1);
    head->prev = NULL;
    head->next = NULL;
    headptr = head;
    char * messageBuffer=  malloc(sizeof(char) * (MAX+5+MAX));
    int messageLength;

     /* Bind socket to local address */
    memset((char *)&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = htonl(INADDR_ANY); 
    serv_addr.sin_port = htons(atoi(argv[2])); /*port from cml arg*/
    
    if ((listenfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) 
    {
        perror("server: can't open stream socket");
        return(1);
    }

    if(setsockopt(listenfd, SOL_SOCKET,(SO_REUSEPORT | SO_REUSEADDR),(char*)&option,sizeof(option)) < 0)
    {
		perror("ERROR: setsockopt failed");
        return EXIT_FAILURE;
	}
    
    /* Bind */
    if(bind(listenfd, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) < 0) 
    {
        perror("ERROR: Socket binding failed");
        return EXIT_FAILURE;
    }

    /* Listen */
    if (listen(listenfd, 10) < 0) 
    {
        perror("ERROR: Socket listening failed");
        return EXIT_FAILURE;
	}
    printf("=== WELCOME TO THE CHATROOM ===\n");
    /*
    Creating worker threads for clients
    */
    while(1)
    {
        socklen_t clilen = sizeof(cli_addr);
		acceptedfd = accept(listenfd, (struct sockaddr*)&cli_addr, &clilen);

		/* Check if max clients is reached */
		if((cli_count + 1) == MAX_CLIENTS){
			printf("Max clients reached. Please try again later: ");
			printf(":%d\n", cli_addr.sin_port);
			close(acceptedfd);
			continue;
		}

		/* Client settings */
		struct client *cli = (struct client *)malloc(sizeof(struct client));
		cli->address = cli_addr;
		cli->sockfd = acceptedfd;
		cli->uid = uid++;

		/* Add client to the queue and fork thread */
		pthread_create(&tid, NULL, &handle_client, (void*)cli);

		/* Reduce CPU usage */
		sleep(1);
    }
}

/* Remove clients to queue */
void queue_remove(int uid){
	pthread_mutex_lock(&clients_mutex);
    struct client * currentNode = headptr;
    
    while (currentNode->next != NULL)
    {
        if(currentNode->next->uid == uid)
        {
            if(currentNode->next->next != NULL)
            {
                currentNode->next = currentNode->next->next;
            }
            else
            {
                strncpy(currentNode->next->name, "\0", strlen(currentNode->next->name));
                currentNode->next = NULL;
            }
            break;
        }
        currentNode = currentNode->next;
    }
    /*
    fprintf(stderr, "before second while from qRemove\n");
    struct client * check = headptr;
    while(check->next != NULL)
    {
        fprintf(stderr, "in second while from qRemove\n");
        if(check->next->uid == uid)
        {
            fprintf(stderr, "found even after removing, name is %s\n", check->next->name);
            break;
        }
        check= check->next;
        fprintf(stderr, "didnt find matching uid");
    }
    */
	pthread_mutex_unlock(&clients_mutex);
}

/* Send message to all clients except sender */
void send_message(char *s, int uid)
{
	pthread_mutex_lock(&clients_mutex);
    struct client * currentNode = headptr;
    while (currentNode->next != NULL)
    {
        if(write(currentNode->next->sockfd, s, strlen(s)) < 0)
        {
            perror("ERROR: write to descriptor failed");
            break;
        }
        currentNode = currentNode->next;
    }
	pthread_mutex_unlock(&clients_mutex);
}

//gets rid of the newline charatcter at the end of stdin
void str_trim_lf (char* arr, int length) 
{
  int i;
  for (i = 0; i < length; i++) { // trim \n
    if (arr[i] == '\n') {
      arr[i] = '\0';
      break;
    }
  }
}

/* Handle all communication with the client */
void *handle_client(void *arg)
{
	char buff_out[BUFFER_SZ];
	char name[MAX];
	int leave_flag = 0;
    int e;
	
	struct client *cli = (struct client *)arg;
    clients[cli_count] = cli->sockfd;
    cli_count++;
    //continue here
    //lock the list to add the client
    pthread_mutex_lock(&clients_mutex);
    struct client * currentNode = headptr;
    while (currentNode->next != NULL)
    {
        currentNode = currentNode->next;
    }
    currentNode->next = cli;
    cli->next = NULL;
	pthread_mutex_unlock(&clients_mutex);
    //release lock

	// Name
	if(recv(cli->sockfd, name, MAX, 0) <= 0 || strlen(name) >= 100)
    {
		printf("Didn't enter the name correctly.\n");
		leave_flag = 1;
	}
    else
    {
        //handle duplicate names
        int dupFlag=0;
        while ((e = refuse_duplicates(name)) != 0)
        {
            dupFlag=1;
            memset(name, 0, sizeof(name));
            int written = write(cli->sockfd, "0", sizeof(char));
            bzero(name, MAX);
            int received = recv(cli->sockfd, name, MAX, 0);
        }
        int writtenSafe = write(cli->sockfd, "1", sizeof(char));
        

        //copy the client's name
		strncpy(cli->name, name, 100);
        if(cli_count ==1)
        {
            sprintf(buff_out, "You are the first user to join the chat!\n");
            //first user msg
        }
        else
        {
		    sprintf(buff_out, "%s has joined the chat!\n", cli->name);
            //all other users msg
        }
		printf("%s", buff_out);
		send_message(buff_out, cli->uid); 
	}
    //clear buffer
	bzero(buff_out, BUFFER_SZ);
	while(1)
    {
		if (leave_flag) 
        {
			break;
		}

		int receive = recv(cli->sockfd, buff_out, BUFFER_SZ, 0);
		if (receive > 0){
			if(strlen(buff_out) > 0){
				send_message(buff_out, cli->uid);

				str_trim_lf(buff_out, strlen(buff_out));
				printf("%s -> %s\n", buff_out, cli->name);
			}
		} 
        else if (receive == 0 || strncmp(buff_out, "exit", 4) == 0)
        {
			sprintf(buff_out, "%s has left\n", cli->name);
			printf("%s", buff_out);
			send_message(buff_out, cli->uid);
			leave_flag = 1; //flag set
		} 
        else 
        {
			printf("ERROR: -1\n");
			leave_flag = 1;
		}

		bzero(buff_out, BUFFER_SZ);
	}

  /* Delete client from queue and yield thread */
	close(cli->sockfd);
    queue_remove(cli->uid);
    free(cli);
    cli_count--;
    pthread_detach(pthread_self());
	return NULL;
}

/*registers the server with the directory server using to remove it*/
void registerToRemove(char * topic)
{
    int sentTopic;
    int recvDup;
    int servlen;
    int sockfdR;
    char               s[MAX];
    int sentPort;
    char req;
    int sendPreShut;
    int connectResult;
    int i;

    /*struct sockaddr_in  cliu_addr, servu_addr;*/

    memset((char *) &servu_addr, 0, sizeof(servu_addr));
    servu_addr.sin_family      = AF_INET;
    servu_addr.sin_addr.s_addr = inet_addr(SERV_HOST_ADDR);
    servu_addr.sin_port        = htons(SERV_TCP_PORT);


    char sending [MAX+5];
    /*popen hostnet -1*/
    if ((sockfdR =  socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("server: can't open stream socket directory server");
        exit(1);
    }
    /* Connect to the server. */
    connectResult = connect(sockfdR, (struct sockfd *) &servu_addr,sizeof(servu_addr));
    if (connectResult < 0)
    {
        perror("Client did not connect to server");
        return(1);
    }
    memset(s, 0, MAX); /*reset s*/
    int identityWritten = send(sockfdR, topic, strlen(topic),0);
    return;
}

/*registers the server with the directory server using UDP links*/
void registerServer(char * topic, char * port, char * ip)
{
    int sentTopic;
    int recvDup;
    int servlen;
    int sockfdR;
    char               s[MAX];
    int sentPort;
    char duped[43];
    char req;
    int sendPreShut;
    int connectResult;
    int i;
    portSelected = atoi(port);
    char allThree[125];
    

    /*struct sockaddr_in  cliu_addr, servu_addr;*/

    memset((char *) &servu_addr, 0, sizeof(servu_addr));
    servu_addr.sin_family      = AF_INET;
    servu_addr.sin_addr.s_addr = inet_addr(SERV_HOST_ADDR);
    servu_addr.sin_port        = htons(SERV_TCP_PORT);


    char sending [MAX+5];
    /*popen hostnet -1*/
    if ((sockfdR =  socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("server: can't open stream socket directory server");
        exit(1);
    }
    /* Connect to the server. */
    connectResult = connect(sockfdR, (struct sockfd *) &servu_addr,sizeof(servu_addr));
    if (connectResult < 0)
    {
        perror("Client did not connect to server");
        return(1);
    }
    memset(s, 0, MAX); /*reset s*/
    char * dupedT = malloc(sizeof(char) * 2);

    snprintf(allThree,125, "S,%s,%s,%s", topic, port, ip);
    int identityWritten = send(sockfdR, allThree, strlen(allThree),0);

    int bytesRead = recv(sockfdR, dupedT, sizeof(char),0);
   
    if(strncmp(dupedT, "D", 1) ==0)
    {
        fprintf(stderr, "A chat room with this name already exists. Try again! Exiting ...");
        fprintf(stderr, dupedT);
        exit(1);
    }
    sockFdDirectory=sockfdR;
    
    free(dupedT);
    return;
}

