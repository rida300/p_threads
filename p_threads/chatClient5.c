#include <time.h>
#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <netinet/in.h>
#include <netdb.h>
#include <string.h>
#include "inet.h"
#include <malloc.h> /*FOR MEMORY ALLOCATION */
#include <resolv.h> /*server to find out the runner's IP address*/ 
#include <unistd.h>  /*FOR USING FORK for at a time send and receive messages*/ 
#include <pthread.h> //use p_thread library

#define FAIL    -1 /*for error output == -1 */
#define MAX   100
#define BUFFSIZE 2048

// Global variables
int globalIndex;
volatile sig_atomic_t flag = 0;
int sockfd = 0;
char name[MAX];
pthread_mutex_t clients_mutex = PTHREAD_MUTEX_INITIALIZER;
char * ip;

//decoration
void str_overwrite_stdout() 
{
  printf("%s", "> ");
  fflush(stdout);
}

//removes newline characters from the end of the input read
void trimString (char* arr, int length) 
{
  int i;
  for (i = 0; i < length; i++) 
  { // trim \n
    if (arr[i] == '\n') 
    {
      arr[i] = '\0';
      break;
    }
  }
}

void catch_ctrl_c_and_exit(int sig) 
{
    flag = 1;
}

/*sends msgs to the server so they can be broadcasted*/
void send_msg() 
{
  char message[BUFFSIZE] = {};
  char buffer[BUFFSIZE + 32] = {};

  while(1) 
  {
  	str_overwrite_stdout();
    fgets(message, BUFFSIZE, stdin);
    trimString(message, BUFFSIZE);

    if (strcmp(message, "exit") == 0) 
    {
		break;
    } 
    else 
    {
      sprintf(buffer, "%s: %s\n", name, message);
      send(sockfd, buffer, strlen(buffer), 0);
    }

	bzero(message, BUFFSIZE);
    bzero(buffer, BUFFSIZE + 32);
  }
  catch_ctrl_c_and_exit(2);
}

/*method to keep receving from server's broadcast*/
void recv_msg() 
{
	char message[BUFFSIZE] = {};
    while (1) 
    {
        int receive = recv(sockfd, message, BUFFSIZE, 0);
        if (receive > 0) 
        {
            int cmp = strncmp(message, ":", 1);
            if(cmp ==0)
            {
                exit(0);
            }
            printf("%s", message);
            str_overwrite_stdout();
        } 
        else if (receive == 0) 
        {
            break;
        } 
        else 
        {
                // nothing
        }
        memset(message, 0, sizeof(message));
    }
}

/*registering with directory server to receive list of chatrooms, uses UDP*/

int showList(char ** servers, int servCount)
{
    ip = malloc(sizeof(char)*14);
    printf("Please select a port from the list to join a Chat Room:\n");
    char * singleRoom = malloc(sizeof(char)* 135);
    char * localTopic = malloc(sizeof(char)*MAX);
    char * localPort = malloc(sizeof(char)*5);
    
    for(int i = 0; i < servCount; i++)
    {
        strcpy(singleRoom, servers[i]+2); //E:topic
        strcpy(localTopic, strtok(singleRoom, ","));
        strcpy(localPort, strtok(NULL, ","));
        strcpy(ip, strtok(NULL, ","));
        printf("Chat Room Topic: %s \t Corresponding Port: %s\n", localTopic, localPort);
    }
    char portChosen[5];
    scanf("%s", &portChosen);
    //clear stdin
    if(!(strchr(portChosen, '\n')))
    {
        scanf("%*[^\n]");
        scanf("%*c");
    } 
    else
    {
        //remove newlie at the nd of stdin
        if(*(portChosen+strlen(portChosen)-1)=='\n')
        *(portChosen+strlen(portChosen)-1) = '\0';
    }
    free(singleRoom);
    free(localPort);
    free(localTopic);
    return atoi(portChosen);
}

int registerWithDirectory()
{
    int index=0;
    int                sockfdR;
    int connectResult;
    struct sockaddr_in cli_addr, serv_addr;
    char               s[MAX];
    int                nread;
    int client_inputFD = fileno(stdin);
    int                i;
    int clilen = sizeof(cli_addr);
    int servlen = sizeof(serv_addr);    
    char singleElement[MAX+15];
    char stillReceiving;
    
    char request[126];
    
    int servCount = 0;

    /*memset(address to start filling memory at,
             value to fill it with,
             number of bytes to fill)*/
    memset((char *)&serv_addr, 0, sizeof(serv_addr)); /*reset the serv_addr, then reassign*/
    serv_addr.sin_family      = AF_INET;
    serv_addr.sin_addr.s_addr = inet_addr(SERV_HOST_ADDR);
    serv_addr.sin_port        = htons(SERV_TCP_PORT);

    char sending [MAX+5];
    if ((sockfdR = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("server: can't open stream socket directory server");
        exit(1);
    }
    
    /* Connect to the server. */
    connectResult = connect(sockfdR, (struct sockaddr *) &serv_addr,sizeof(serv_addr));
    if (connectResult < 0)
    {
        perror("Client did not connect to server");
        exit(1);
    }
    /* connection was successfully established by this point*/

    memset(s, 0, MAX); /*reset s*/
    char * imClient = "C";
    int sentIdentity = send(sockfdR, imClient, strlen(imClient),0);
    /*If the user enters anything longer than the port number or
    something that cant be parsed to a port number, the client will be
    disconnected.*/

    char * response = malloc(sizeof(char)*135);
    char **servers = malloc(sizeof(char *)*15); //maximum 15 chat rooms can be read
    int bigStringBytes = recv(sockfdR, response, sizeof(char)*135,0);
    //memset(response, 0, sizeof(response));
    //check if connection still alive
    int error = 0;
    socklen_t len = sizeof (error);
    int retval = getsockopt (sockfdR, SOL_SOCKET, SO_ERROR, &error, &len);
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
    //reads all chat room from directory server
    while(response[0] != 'E')
    {
        pthread_mutex_lock(&clients_mutex);
        servers[servCount] = malloc(strlen(response)+1);
        strcpy(servers[servCount], response);
        servCount++;
        pthread_mutex_unlock(&clients_mutex);
        memset(response, 0, sizeof(response));
        int more = recv(sockfdR, response, sizeof(char)*135,0);
    }
    //clls method to display all chat rooms
    int res = showList(servers, servCount);
    close(sockfdR);
    return res;
}

int main(int argc, char ** argv )
{
    struct sockaddr_in serv_addr;
    int client_inputFD = fileno(stdin);
    int                i;
    int connectResult;
    int portFromRegister=registerWithDirectory();

    signal(SIGINT, catch_ctrl_c_and_exit);


    /*socket settings*/
    memset((char *)&serv_addr, 0, sizeof(serv_addr)); /*reset the serv_addr, then reassign*/
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = inet_addr(ip);
    serv_addr.sin_port = htons(portFromRegister);//fix this

    /* Create the socket. */
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0)
    {
        perror("socket (client)");
        return(1);
    }

    /* Connect to the server. */
    connectResult = connect(sockfd, (struct sockaddr *) &serv_addr,sizeof(serv_addr));
    if (connectResult < 0)
    {
        perror("Client did not connect to server");
        return(1);
    }
    
    /*get client's name*/

    fprintf(stderr, "Please enter your name: ");
    fgets(name, 100, stdin);    
    
    trimString(name, strlen(name));
    if (strlen(name) > 100 )
    {
		printf("Name must be less than 100.\n");
		return EXIT_FAILURE;
	}
    // Send name to server
	send(sockfd, name, 100, 0);
    char dup[1];
    int received = recv(sockfd, dup, sizeof(dup), 0);

    while(strncmp(dup, "0", 1) == 0)
    {
        memset(name, 0, sizeof(name));
        fprintf(stderr, "This name is taken. Please choose another one\n");
        
        fgets(name, 100, stdin);
        trimString(name, strlen(name));
        if (strlen(name) > 100 )
        {
            printf("Name must be less than 100.\n");
            return EXIT_FAILURE;
        }
        
        send(sockfd, name, 100, 0);
        received = recv(sockfd, dup, sizeof(dup), 0);
    }

	printf("=== WELCOME TO THE CHATROOM ===\n");
    
    //thread for the sending of msgs to the server so they can be broadcasted
    pthread_t send_msg_thread;
    if(pthread_create(&send_msg_thread, NULL, (void *) send_msg, NULL) != 0)
    {
		printf("ERROR: pthread\n");
        return EXIT_FAILURE;
	}
    //thread for reading stdin
	pthread_t recv_msg_thread;
    if(pthread_create(&recv_msg_thread, NULL, (void *) recv_msg, NULL) != 0)
    {
		printf("ERROR: pthread\n");
		return EXIT_FAILURE;
	}

	while (1)
    {
		if(flag)
        {
			printf("\nBye!!\n");
			break;
        }
	}
	close(sockfd);
	return EXIT_SUCCESS;
}