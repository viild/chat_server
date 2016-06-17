#include <stdio.h> 
#include <stdlib.h> 
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/un.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <netdb.h>
#include <string.h>
#include <pthread.h>
#include <time.h>

#define PORT 2232
#define MAX_LENGTH 290
#define Sadr (struct sockaddr *)
#define MAX_CLIENTS 25000


FILE * log = NULL;
struct user
{
	int UID;
	int Socket;
	char nickname[11];
};

pthread_mutex_t log_mutex;
pthread_mutex_t arr_mutex;
struct user Users[MAX_CLIENTS];

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
	int ClientIdx = 0;
	int i_idx = 0;
	int SockFD;
	int UID_to = 0;
	int UID_from = 0;
	int UID_to_idx = 0;
	char message[MAX_LENGTH];
	char name[20];
	char UID[10];
	char msg_for_send[256];
	
	SockFD = *((int *) arg);
	free(arg);

	pthread_detach(pthread_self());

	recv(SockFD, name, 20, 0); //Get Username

	//Find first empty slot for user
	pthread_mutex_lock(&arr_mutex);
		for(i_idx = 0; i_idx < MAX_CLIENTS; i_idx++)
		{
			if(Users[i_idx].UID == 0)
			{
				ClientIdx = i_idx;
				break;	
			}
		}
		Users[ClientIdx].UID = i_idx+1;
		Users[ClientIdx].Socket = SockFD;
		strcpy(Users[ClientIdx].nickname, name);
	pthread_mutex_unlock(&arr_mutex);
	

	//Send to USER his UID
	itoa(Users[ClientIdx].UID, UID, 10);
	send(SockFD, UID, 10, 0);
	

	while(1)
	{
		int recvln = 0;
		memset(&message, 0, sizeof(message));
		if(recvln = recv(SockFD, message, MAX_LENGTH, 0) == -1)
		{
			break;
		}
		if(strlen(message) > 0)
		{
			if(strcmp(message, "exit") == 0) break;
	
			parser(&message, &UID_from, &UID_to, &msg_for_send);

			pthread_mutex_lock(&arr_mutex);
			int ready1 = 0;
			int ready2 = 0;
			memset(&message, 0, sizeof(message));
			for(i_idx = 0; i_idx < MAX_CLIENTS; i_idx++)
			{
				//build new message "Nickname[UID]:message"
				if(Users[i_idx].UID == UID_from)
				{
					itoa(Users[i_idx].UID, UID, 10);
					strcat(message, Users[i_idx].nickname);
					strcat(message, "[");
					strcat(message, UID);
					strcat(message, "]:");
					strcat(message, msg_for_send);
					strcat(message, "\n");
					ready1 = 1;
					if((ready1 * ready2) == 1) break;
				}
				//take index if message for single user
				else if(UID_to != 0 && Users[i_idx].UID == UID_to)
				{
					UID_to_idx = i_idx;
					ready2 = 1;
					if((ready1 * ready2) == 1) break;
				}
			}
			pthread_mutex_unlock(&arr_mutex);

			//log
			pthread_mutex_lock(&log_mutex);
			fprintf(log, "%s", message);	
			pthread_mutex_unlock(&log_mutex);

			//resend
			pthread_mutex_lock(&arr_mutex);
			//if message for server
			if(UID_to == 0)
			{
				for(i_idx = 0; i_idx < MAX_CLIENTS; i_idx++)
				{
					send(Users[i_idx].Socket, message, MAX_LENGTH, 0);
					send(Users[i_idx].Socket, "publ", sizeof("publ"), 0);
				}
			}
			//if message for single user
			else
			{
				//check for the existence of the user
				int isExist = 0;
				for(i_idx = 0; i_idx < MAX_CLIENTS; i_idx++)
				{
					if(Users[i_idx].UID == UID_to)
					{
						isExist = 1;
						break;
					}
				}
				//if isn't exist
				if(isExist == 0)
				{
					send(Users[ClientIdx].Socket, "The UID isn't exist", MAX_LENGTH, 0);	
					continue;
				}
				send(Users[UID_to_idx].Socket, message, MAX_LENGTH, 0);	
				send(Users[UID_to_idx].Socket, "priv", sizeof("priv"), 0);
			}		
			pthread_mutex_unlock(&arr_mutex);
		}
		//if message read error
		else
		{
			break;
		}
	}

	pthread_mutex_lock(&arr_mutex);
		Users[ClientIdx].UID = 0;
		Users[ClientIdx].Socket = 0;
		memset(&Users[ClientIdx].nickname, 0, sizeof(Users[ClientIdx].nickname));
	pthread_mutex_unlock(&arr_mutex);

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
			perror("\nTCP Error: accept");
			shutdown(msock, SHUT_RDWR);
			close(msock);
			continue;
		}
		arg = malloc(sizeof(int));
		*arg = ConnectionFD;
		pthread_create(&mythread, 0, TCPcomm, arg); 	
	}
	return NULL;
}

int main()
{
	//Variables
	int i_idx;
	int * TCParg;
	int TCPSocketFD;
	struct sockaddr_in TCPServer;
	struct tm * timeinfo;
	char File_name[60];
	char option[50];
	time_t rawtime;
	//Threads
	pthread_t TCPthread;
	//ArrMutex
	if(pthread_mutex_init(&arr_mutex, 0) != 0)
	{
		perror("\nArray mutex error:init");
		exit(EXIT_FAILURE);
	}
	//log mutex
	if(pthread_mutex_init(&log_mutex, 0) != 0)
	{
		perror("\nLog mutex error:init");
		exit(EXIT_FAILURE);
	}
	//Create TCP scoket
	if((TCPSocketFD = socket(PF_INET, SOCK_STREAM, 0)) == -1)
	{
		perror("\nTCP Error:create");
		close(TCPSocketFD);
		exit(EXIT_FAILURE);
	}
	printf("\nTCP Socket has been created");
	//Clean up
	memset(&Users, 0, sizeof(Users));
	memset(&TCPServer, 0, sizeof(TCPServer));
	//initialize
	TCPServer.sin_family = PF_INET;
	TCPServer.sin_port = htons(PORT);
	TCPServer.sin_addr.s_addr = htonl(INADDR_ANY);
	//Bind TCP socket
	if(bind(TCPSocketFD, Sadr &TCPServer, sizeof(TCPServer)) == -1)
	{
		perror("\nTCP Error:bind");
		close(TCPSocketFD);
		exit(EXIT_FAILURE);	
	}
	printf("\nTCP Socket has been bound");
	//Create log file
	memset(&File_name, 0, sizeof(File_name));
	time(&rawtime); 
	timeinfo = localtime(&rawtime);
	system("mkdir logs");
	strcat(File_name, "logs/");
	strcat(File_name, asctime(timeinfo));
	log = fopen(File_name, "wt"); //creating text file for write
	if (! log) 
	{
		perror("\nFile error:create");
		log = NULL;
		close(TCPSocketFD);
		exit(EXIT_FAILURE);
	}
	printf("\nLog opened");
	//Create TCP Thread
	TCParg = malloc(sizeof(int));
	*TCParg = TCPSocketFD;
	pthread_create(&TCPthread, 0, TCPThrd, TCParg);
	//listen socket
	if(listen(TCPSocketFD, SOMAXCONN) == -1)
	{
		perror("\nTCP Error:listen");
		close(TCPSocketFD);
		exit(EXIT_FAILURE);
	}
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
			printf("\nWait for other...");
			pthread_mutex_lock(&arr_mutex);
			for(i_idx = 0; i_idx < MAX_CLIENTS; i_idx++)
			{
				if(Users[i_idx].Socket != 0)
				{
					shutdown(Users[i_idx].Socket, SHUT_RDWR);
					close(Users[i_idx].Socket);
				}
			}
			pthread_mutex_unlock(&arr_mutex);
			break;
		}
		if(strcmp(option, "print") == 0)
		{
			for(i_idx = 0; i_idx < 10; i_idx++)
			{
				printf("\n%d\t%d\t%s", Users[i_idx].UID, Users[i_idx].Socket, Users[i_idx].nickname);
			}
		}
	}
	//End
	pthread_mutex_destroy(&log_mutex);
	pthread_mutex_destroy(&arr_mutex);
	close(TCPSocketFD);
	if(fclose(log) != 0)
		perror("\nFile error:close\n");
	else printf("\nLog closed\n");
	log = NULL;
	return EXIT_SUCCESS;	
}

