/* --------BEGIN LIBRARY SECTION---------- */
#include <stdio.h> 
#include <stdlib.h> 
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <string.h>
#include <pthread.h>
#include <time.h>

#include "ErrorCodes.h"
/* -------END LIBRARY SECTION--------------- */

/* -------BEGIN DEFINITIONS SECTION---------- */

/* Main port for server's socket */
#define PORT 2232
/* Maximum length of user message */
#define MAX_LENGTH 290
/* Shortest definition of (struct sockaddr *) */
#define Sadr (struct sockaddr *)
/* Maximum of clients */
#define MAX_CLIENTS 25000


/* -------END DEFINITIONS SECTION----------- */

/* ------------------------------------BEGIN GLOBAL DEFINITIONS------------------------------------------- */
/*
This section contains global data for the process. This section contains global pointers, variables, mutexes, etc.
All of these definitions are visible in any functions.
*/

/* Pointer to the log file */
FILE * log = NULL;
/* Structure of a user information for handling by the server */
struct user
{
	int UID; //User ID
	int Socket; //FD of the user socket
	char nickname[11]; //user nickname
};

/* Mutexes */				
pthread_mutex_t log_mutex; //mutex of the log file
pthread_mutex_t db_mutex; //mutex of the user database

/* This is structure of users for MAX_CLIENTS person */
struct user Users[MAX_CLIENTS];





/* ------------------------------------END GLOBAL DEFINITIONS----------------------------------------------- */





/* -----------------------------------------BEGIN FUNCTION HEADER----------------------------------------- */
/*Function which convert integer to ASCII*/
char * itoa(int number, char * destination, int base);
/* Message handler. When message incoming server extracts destination user nickname and message */
void parser(char (*message)[], int *UID_from,int *UID_to, char (*returned_message)[]);
/* TCPThrd accepts incoming connections, TCPcomm processing such connections */
void * TCPcomm(void * arg);
void * TCPThrd(void * sock);
/* Send message to all users */
int send_to_all(char message[]);
/* Send message to particular user */
int send_to_user(char message[], int source_fd, int destination_id);


/* ------------------------------------------END FUNCTION HEADER------------------------------------------ */





/* ======================================IMPLEMENTATION============================================= */
/* All realisations in the file are below */
int main()
{
	/* ====[VARIABLES]========*/
	int i_idx;
	int * TCParg;
	int TCPSocketFD;
	struct sockaddr_in TCPServer;
	struct tm * timeinfo;
	char File_name[60];
	char option[50];
	time_t rawtime;
	/* ===============[Thread variables]============ */
	pthread_t TCPthread;
	/* ===============[CLEAN UP]==================== */
	memset(&Users, 0, sizeof(Users));
	memset(&TCPServer, 0, sizeof(TCPServer));
	memset(&File_name, 0, sizeof(File_name));
	/* ===========[INITIALISATIONS]===================*/
	//Database mutex init
	if(pthread_mutex_init(&db_mutex, 0) != 0)
	{
		perror("Database mutex error:init");
		exit(DB_INIT_ERR);
	}
	//Log mutex init
	if(pthread_mutex_init(&log_mutex, 0) != 0)
	{
		perror("Log mutex error:init");
		exit(LOG_INIT_ERR);
	}
	//Create TCP scoket
	if((TCPSocketFD = socket(AF_INET, SOCK_STREAM, 0)) == -1)
	{
		perror("TCP Error:create");
		close(TCPSocketFD);
		exit(SOCK_CRT_ERR);
	}
	printf("TCP Socket has been created\n");	
	//initialize TCPServer structure
	TCPServer.sin_family = AF_INET;
	TCPServer.sin_port = htons(PORT);
	TCPServer.sin_addr.s_addr = htonl(INADDR_ANY);	
	//Bind TCP socket
	if(bind(TCPSocketFD, Sadr &TCPServer, sizeof(TCPServer)) == -1)
	{
		perror("TCP Error:bind");
		close(TCPSocketFD);
		exit(SOCK_BIND_ERR);	
	}
	printf("TCP Socket has been bound\n");
	//Create log file
	time(&rawtime); 
	timeinfo = localtime(&rawtime);
	system("mkdir logs");
	strcat(File_name, "logs/");
	strcat(File_name, asctime(timeinfo));
	log = fopen(File_name, "wt"); //creating text file for write
	if (!log) 
	{
		perror("Log file error:create");
		log = NULL;
		fclose(log);
		close(TCPSocketFD);
		exit(LOGF_CRT_ERR);
	}
	printf("Log opened\n");
	//Create TCP Thread which processing all of incoming connections
	TCParg = malloc(sizeof(int));
	*TCParg = TCPSocketFD;
	pthread_create(&TCPthread, 0, TCPThrd, TCParg);
	//Set TCPSocketFD status as listener for five clients in times
	if(listen(TCPSocketFD, SOMAXCONN) == -1)
	{
		perror("TCP Error:listen");
		close(TCPSocketFD);
		log = NULL;
		fclose(log);
		exit(SOCK_LST_ERR);
	}
	/* =============[END OF INITIALISATIONS]================== */
	//Main cycle
	printf("\nServer is enable\n");
	while(1)
	{
		memset(&option, 0, sizeof(option));
		printf("Type a command: ");
		scanf("%s", option);
		if(strcmp(option, "close") == 0)
		{
			int i_idx;
			printf("Wait until server close all connections\n");
			pthread_mutex_lock(&db_mutex);
			for(i_idx = 0; i_idx < MAX_CLIENTS; i_idx++)
			{
				if(Users[i_idx].Socket != 0)
				{
					close(Users[i_idx].Socket);
				}
			}
			pthread_mutex_unlock(&db_mutex);
			break;
		}
		if(strcmp(option, "print") == 0)
		{
			for(i_idx = 0; i_idx < 10; i_idx++)
			{
				printf("%d\t%d\t%s\n", Users[i_idx].UID, Users[i_idx].Socket, Users[i_idx].nickname);
			}
		}
	}
	//End
	pthread_mutex_destroy(&log_mutex);
	pthread_mutex_destroy(&db_mutex);
	close(TCPSocketFD);
	if(fclose(log) != 0)
		perror("File error:close\n");
	else printf("Log closed\n");
	log = NULL;
	return EXIT_SUCCESS;	
}

char *itoa(int number, char *destination, int base) 
{
	int count = 0;
	do 
	{
		int digit = number % base;
		destination[count++] = (digit > 9) ? digit - 10 +'A' : digit + '0';
  	} while ((number /= base) != 0);
	destination[count] = '\0';
	int i;
	for (i = 0; i < count / 2; ++i) 
	{
		char symbol = destination[i];
		destination[i] = destination[count - i - 1];
		destination[count - i - 1] = symbol;
  	}
  return destination;
}

void parser(char (*message)[], int *UID_from,int *UID_to, char (*returned_message)[])
{
	char buffer[MAX_LENGTH];
	int i_idx = 0;
	int j_idx = 0;
	memset(&buffer, 0, sizeof(buffer));
	while((*message)[i_idx] != ';')
	{
		buffer[j_idx] = (*message)[i_idx];
		j_idx++;
		i_idx++;
	}
	j_idx = 0;
	i_idx++;
	*UID_from = atoi(buffer);
	memset(&buffer, 0, sizeof(buffer));
	while((*message)[i_idx] != ';')
	{
		buffer[j_idx] = (*message)[i_idx];
		j_idx++;
		i_idx++;
	}
	j_idx = 0;
	i_idx++;
	*UID_to = atoi(buffer);
	memset(&buffer, 0, sizeof(buffer));
	while(i_idx < MAX_LENGTH)
	{
		buffer[j_idx] = (*message)[i_idx];
		i_idx++;
		j_idx++;
	}
	strcpy(*returned_message, buffer);

}

void *TCPcomm(void *arg)
{
	//variables
	int i_idx = 0;
	int SockFD; //FD of user socket
	int source_id = 0; //ID of particular user
	int destination_id = 0; //ID of some of destination user
	int user_index = 0; //user index in database
	int usr = 0; //temp variable
	char message[MAX_LENGTH];
	char name[11]; //user nickname
	char UID[10];
	char msg_for_send[256];
	
	SockFD = *((int *) arg);
	free(arg);

	pthread_detach(pthread_self());

	recv(SockFD, name, 11, 0); //Get Username
	printf("Username: %s\n", name);

	//Find first empty slot for user
	pthread_mutex_lock(&db_mutex);
	for(i_idx = 0; i_idx < MAX_CLIENTS; i_idx++)
	{
		if(Users[i_idx].UID == 0)
		{
			Users[i_idx].UID = i_idx+1;
			Users[i_idx].Socket = SockFD;
			strcpy(Users[i_idx].nickname, name);
			user_index = i_idx;
			source_id = i_idx + 1;
			break;	
		}
	}
	pthread_mutex_unlock(&db_mutex);
	
	memset(&UID, 0, sizeof(UID));
	//Send to USER his UID
	itoa(SockFD, UID, 10);
	if(send(SockFD, UID, 10, 0) <= 0)
	{
		pthread_mutex_lock(&db_mutex);
		Users[user_index].UID = 0;
		Users[user_index].Socket = -3;
		memset(&Users[user_index].nickname, 0, sizeof(Users[user_index].nickname));
		pthread_mutex_unlock(&db_mutex);
		printf("Socket was closed\n");
		close(SockFD);
		return NULL;
	}
	while(1)
	{
		memset(&message, 0, sizeof(message));
		if(recv(SockFD, message, MAX_LENGTH, 0) == -1)
		{
			break;
		}
		printf("Recieved message is:%s\n", message);
		if(strlen(message) <= 0)
		{
			break;
		}
		else
		{
			if(strcmp(message, "exit") == 0) break;
	
			parser(&message, &usr, &destination_id, &msg_for_send);

			pthread_mutex_lock(&db_mutex);
			//build new message "Nickname[UID]:message"
			memset(&message, 0, sizeof(message));
			strcat(message, Users[user_index].nickname);
			strcat(message, "[");
			strcat(message, UID);
			strcat(message, "]:");
			strcat(message, msg_for_send);
			strcat(message, "\n");
			
			int isExist = 0;
			for(i_idx = 0; i_idx < MAX_CLIENTS; i_idx++)
			{
				if(Users[i_idx].UID == destination_id) isExist = 1;
			}

			pthread_mutex_unlock(&db_mutex);

			//log
			pthread_mutex_lock(&log_mutex);
			fprintf(log, "%s", message);	
			pthread_mutex_unlock(&log_mutex);

			//resend
			//if message for server
			if(destination_id == 0)
			{
				send_to_all(message);
			}
			//if message for single user
			if(isExist == 1 && destination_id != 0) 
			{
				send_to_user(message, SockFD, destination_id);
			}	
		}
	}

	pthread_mutex_lock(&db_mutex);
		Users[user_index].UID = 0;
		Users[user_index].Socket = 0;
		memset(&Users[user_index].nickname, 0, sizeof(Users[user_index].nickname));
	pthread_mutex_unlock(&db_mutex);

	close(SockFD);
 
	return NULL;
}

void *TCPThrd(void *sock)
{
	int ConnectionFD;	
	int msock;
	int * arg;
	pthread_t mythread;
	msock = *((int *) sock);
	free(sock);
	pthread_detach(pthread_self());
	while(1)
	{
		if((ConnectionFD = accept(msock, 0, 0)) == -1)
		{
			perror("TCP Error: accept\n");
			close(msock);
			//TO DO Log this event
			continue;
		}
		arg = malloc(sizeof(int));
		*arg = ConnectionFD;
		pthread_create(&mythread, 0, TCPcomm, arg); 	
	}
	return NULL;
}

int send_to_all(char message[])
{
pthread_mutex_lock(&db_mutex);
	int i_idx = 0;
	for(i_idx = 0; i_idx < MAX_CLIENTS; i_idx++)
	{
		if(Users[i_idx].Socket != 0)
		{
			send(Users[i_idx].Socket, message, MAX_LENGTH, 0);
			send(Users[i_idx].Socket, "publ", sizeof("publ"), 0);
		}
	}
pthread_mutex_unlock(&db_mutex);
}

int send_to_user(char message[], int source_fd, int destination_id)
{
int i_idx = 0;
int destination_fd = 0;
pthread_mutex_lock(&db_mutex);
	//check existence of the user
	int isExist = 0;
	for(i_idx = 0; i_idx < MAX_CLIENTS; i_idx++)
	{
		if(Users[i_idx].UID == destination_id)
		{
			isExist = 1;
			destination_fd = Users[i_idx].UID;
			break;
		}
	}
	//if user isn't exist
	if(isExist == 0)
	{
		send(source_fd, "This user is not exitst", sizeof("This user is not exist"), 0);	
		return USR_EXIST_ERR;
	}
	send(destination_fd, message, MAX_LENGTH, 0);	
	send(destination_fd, "priv", sizeof("priv"), 0);
pthread_mutex_unlock(&db_mutex);
}

