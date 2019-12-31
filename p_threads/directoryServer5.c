#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/types.h>
#include "inet.h"
#include <resolv.h>
#include <pthread.h> //use p_thread library
#include "sem.h"

#define MAX 100
#define BUFFER_SZ 2048
#define MAX_CLIENTS 10

char addNewEntity(char *);
int handle_entity(void *); //this is the worker thread method

typedef struct entity {
    char ip[16];
    char port[5];
    char topic[MAX+1];
    int sockfd;
    int uid;
    struct sockaddr_in address;
    struct entity * next;
    struct entity * prev;
}; //represents a server


/*Global Variables*/
static _Atomic unsigned int  cli_count = 0;
static _Atomic unsigned int  server_count = 0;
static int                   uid = 10;
pthread_mutex_t              clients_mutex = PTHREAD_MUTEX_INITIALIZER; //mutex for locking critical sections
struct entity*  headptr = NULL; //the headptr of linked list storing servers
int mutex;                      // semaphore id
int sockfdGlobal;
int sockfd;
char * response;

int main(int argc, char **argv) 
{

    int             option = 1;
    pthread_attr_t  attr;
	int	            clilen, childpid;
	char	        s[MAX];
	char	        request;
	pthread_t       tid;
    struct          sockaddr_in  cli_addr, serv_addr;
    int             listenfd;
    int             acceptedfd;

    struct entity * head = (struct client *) malloc((sizeof(struct entity)));
    strncpy(head->topic, "\0", 1); //initialize first cell to be empty
    strncpy(head->port, "\0", 1);
    strncpy(head->ip, "\0", 1);
    head->prev = NULL;
    head->next = NULL;
    headptr = head; 

    /* Create communication endpoint */
    listenfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) 
    {
        perror("server: couldn't connect to entity");
        return(1);
    }
    /* Bind socket to local address */
    memset((char *) &serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family      = AF_INET;
    serv_addr.sin_addr.s_addr = inet_addr(SERV_HOST_ADDR);
    serv_addr.sin_port        = htons(SERV_TCP_PORT);

    if(setsockopt(listenfd, SOL_SOCKET,(SO_REUSEPORT | SO_REUSEADDR | SO_LINGER),(char*)&option,sizeof(option)) < 0)
    //if(setsockopt(listenfd, SOL_SOCKET,(SO_REUSEPORT | SO_REUSEADDR),(char*)&option,sizeof(option)) < 0)
    {
		perror("ERROR: setsockopt failed");
        return EXIT_FAILURE;
	}

    /*Bind*/
    if (bind(listenfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0) 
    {
        perror("server: can't bind local address");
        return EXIT_FAILURE;
    }
    
    /* Listen */
    if (listen(listenfd, 10) < 0) 
    {
        perror("ERROR: Socket listening failed");
        return EXIT_FAILURE;
	}
    printf("=== WELCOME TO THE DIRECTORY SERVER ===\n");
    
    while(1)
    {
        socklen_t clilen = sizeof(cli_addr);
        acceptedfd = accept(listenfd, (struct sockaddr*)&cli_addr, &clilen);
        /* Check if max clients is reached */
		if((cli_count + 1) == MAX_CLIENTS)
        {
			printf("Max clients reached. Please try again later: ");
			printf(":%d\n", cli_addr.sin_port);
			close(acceptedfd);
			continue;
		}

        /* Client settings */
		struct entity *cli = (struct entity *)malloc(sizeof(struct entity));
		cli->address = cli_addr;
		cli->sockfd = acceptedfd;
		cli->uid = uid++;

        /* Add client to the queue and fork thread */
		pthread_create(&tid, NULL, (void *)&handle_entity, (void*)cli);
        pthread_detach(&tid);
		/* Reduce CPU usage */
		sleep(1);
    }
}

/*
This is the method called when a thread is spawned off. It reads the message 
from the server/client and uses the first char to differentiate between the two.
I have a switch case in order to process the different inputs. 
*/
int handle_entity(void * arg)
{
	char buff_out[BUFFER_SZ];
	char universalMsg[122];
	int leave_flag = 0;
    int e;
	cli_count++; //on;y increment if server , not on client
    int mallocI;
	struct entity *cli = (struct client *)arg;
    struct entity * curr = headptr;
    // struct linger sl;
    // sl.l_onoff = 1;		/* non-zero value enables linger option in kernel */
    // sl.l_linger = 0;	/* timeout interval in seconds */
    // int set = setsockopt(cli->sockfd, SOL_SOCKET, SO_LINGER, &sl, sizeof(sl));
    
    memset(universalMsg, 0, 122);
    int some= cli->sockfd;
    if(recv(cli->sockfd, universalMsg, 122, 0) <= 0 || strlen(universalMsg) >= 122)
    {
		printf("Didn't enter the name correctly.\n");
		leave_flag = 1;
	}
    fprintf(stderr, "read universalMsg %s\n", universalMsg);
    char sendToServer;
    int respondedToServer;
    switch(universalMsg[0])
    {
        //register a server
        case 'S': 
            sendToServer = addNewEntity(universalMsg+2);
            server_count++;
            int errorCon = 0;
            socklen_t lenS = sizeof (errorCon);
            int retvalS = getsockopt (cli->sockfd, SOL_SOCKET, SO_ERROR, &errorCon, &lenS);
    
            if (retvalS != 0) 
            {
            /* there was a problem getting the error code */
            fprintf(stderr, "error getting socket error code: %s\n", strerror(retvalS));
            return;
            }
            if (errorCon != 0) 
            {
                /* socket has a non zero error status */
                fprintf(stderr, "socket error: %s\n", strerror(errorCon));
            }

            respondedToServer = write(some, &sendToServer, sizeof(char),0);
            printf("errno is %s\n", strerror(errno));
            //close(cli->sockfd);
            break;

        //pass chat room info to client
        case 'C':   
            //lock the list 
            pthread_mutex_lock(&clients_mutex);
            int anotherIndex=0;
            response = malloc(sizeof(char)*124);
            while (curr->next != NULL)
            {   
                memset(response, 0, 124);
                //sprintf(allServ[anotherIndex], "G,%s,%s,%s\n", curr->next->topic,curr->next->ip,curr->next->port);
                sprintf(response, "G,%s,%s,%s\n", curr->next->topic,curr->next->port, curr->next->ip);

                fprintf(stderr, "writing response from while %s\n", response); 
                int bigString = write(cli->sockfd, response, sizeof(char)*135);  
                curr = curr->next;
                anotherIndex++;
            };
            pthread_mutex_unlock(&clients_mutex);
            //unlock the linked list
            int error = 0;
            socklen_t len = sizeof (error);
            int retval = getsockopt (cli->sockfd, SOL_SOCKET, SO_ERROR, &error, &len);
            //try using uid to write instead of sockfd
            if (retval != 0) 
            {
            /* there was a problem getting the error code */
            fprintf(stderr, "error getting socket error code: %s\n", strerror(retval));
            return;
            }
            if (error != 0) 
            {
                /* socket has a non zero error status */
                fprintf(stderr, "socket error: %s\n", strerror(error));
            }
            char * end = "E"; //when all chatrooms have been sent, send an E to indicate this
            int writeE = send(cli->sockfd, end, strlen(end),0); //E for end
            fprintf(stderr, "i wrote %s\n", end);
            fprintf(stderr, "writing response E %d\n", writeE); 
            sleep(3);
            //close(cli->sockfd);
            break;
            //removes the server from the linked list
        case 'R':
                removeServer(universalMsg+2);
            break;
    }
    close(cli->sockfd);
    free(cli);
    //add queue_remove here
    //pthread_detach(pthread_self());
	return 0;
}


/**
 removes the server with the given topic (name) from the liked list of all servers 
*/
void removeServer(char * topic) 
{
    if(headptr == NULL)
        return;

    struct entity * curr = headptr;
    pthread_mutex_lock(&clients_mutex);
    while(curr->next != NULL)
    {
        if(strncmp(curr->next->topic, topic, 101) == 0)
        {
            fprintf(stderr, "found the chat room with this topic\n");
            if(curr->next->next != NULL)
            {
                //not last element
                curr->next = curr->next->next;
            }
            else
            {
                //last element
                curr->next = NULL;
            }
            pthread_mutex_unlock(&clients_mutex);
            return;
        }
        curr = curr->next;
    }
    //check if lock is acquired
    pthread_mutex_unlock(&clients_mutex);
}

/*iterates over the linked list to check if the new topic already exists*/
int refuseDuplicates(char * name){
    if(headptr==NULL)
        return -1;
    struct entity * curr = headptr;

    while(curr->next != NULL){
        if( strncmp(curr->next->topic, name, 101) == 0)
            return -1;
        curr = curr->next;
    }
    return 1;
}


/*adds the new server to the linked list of the */
char addNewEntity(char * allThree) 
{
    pthread_mutex_lock(&clients_mutex);
    if(headptr == NULL)
        return(-1);
    struct entity * checkDup = headptr;
    struct entity * curr = headptr;
    char * topicReceived = malloc(sizeof(char) * MAX);
    strncpy(topicReceived, strtok(allThree, ","), 100);
    //checks duplicate topics
    while(checkDup->next != NULL)
    {
        if( strncmp(checkDup->next->topic, topicReceived, 101) == 0)
        {
            pthread_mutex_unlock(&clients_mutex);
             return('D');//returns a 'D' if found duplicate
        }
        checkDup = checkDup->next;
    }
    while(curr->next != NULL)
    {
        curr = curr->next;
    }
    //otherwise, add the server to the list
    struct entity * newEntity = (struct server *)malloc(sizeof(struct entity));
    curr->next = newEntity;
    newEntity->next = NULL; 
    pthread_mutex_unlock(&clients_mutex);

    strncpy(newEntity->topic, topicReceived, 100);
    strncpy(newEntity->port, strtok(NULL, ","), 5);
    strncpy(newEntity->ip, strtok(NULL, ","), 16);
    fprintf(stderr, "topic is %s\n", newEntity->topic);
    fprintf(stderr, "ip is %s\n", newEntity->ip);
    fprintf(stderr, "port is %s\n", newEntity->port);
    free(topicReceived);
    return('G'); //good, no duplicate found
}