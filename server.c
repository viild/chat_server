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
#define TCP_PROT 1
#define UDP_PROT 2
#define UNDEF_PROT 0;


FILE * log = NULL;
struct user
{
	int UID;
	int Socket;
	char nickname[30];
	struct sockaddr_in info;
};

struct cuser
{
	int UID;
	int protocol;
};

pthread_mutex_t log_mutex;
pthread_mutex_t arr_mutex;
pthread_mutex_t cusers_mutex;
struct user Users[MAX_CLIENTS];
struct cuser Connected_users[MAX_CLIENTS];
int users_count = 0;

/*function which compares two structures*/
int Compare2SockAddr(const struct sockaddr_in * first, const struct sockaddr_in * second)
{
	int result = 0;
	if(first->sin_family != second->sin_family) result++;
	if(first->sin_port != second->sin_port) result++;
	if(first->sin_addr.s_addr != second->sin_addr.s_addr) result++;
	if(strcmp(first->sin_zero,second->sin_zero) != 0) result++;
	return result;
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
		pthread_mutex_lock(&cusers_mutex);
		Connected_users[ClientIdx].UID = Users[ClientIdx].UID;
		Connected_users[ClientIdx].protocol = TCP_PROT;
	pthread_mutex_unlock(&cusers_mutex);
	

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
					if(Connected_users[i_idx].UID != UID_to)
					{
						if(Connected_users[i_idx].protocol == TCP_PROT)
						{
							send(Users[i_idx].Socket, message, MAX_LENGTH, 0);
							send(Users[i_idx].Socket, "publ", sizeof("publ"), 0);
						}
						else if(Connected_users[i_idx].protocol == UDP_PROT)
						{
							sendto(Users[i_idx].Socket, message, sizeof(message), 0, 
								(struct sockaddr*)&Users[i_idx].info, 
								sizeof(Users[i_idx].info));
							sendto(Users[i_idx].Socket, "publ", sizeof("publ"), 0, 
								(struct sockaddr*)&Users[i_idx].info, 
								sizeof(Users[i_idx].info));
						}
					}
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
					if(Connected_users[ClientIdx].protocol == TCP_PROT)
					{
						send(Users[ClientIdx].Socket, "The UID isn't exist", MAX_LENGTH, 0);	
					}	
					else if(Connected_users[ClientIdx].protocol == UDP_PROT)
					{
						sendto(Users[ClientIdx].Socket, "The UID isn't exist", strlen(message), 0, 
								(struct sockaddr*)&Users[ClientIdx].info, 
								sizeof(Users[ClientIdx].info));
					}
					continue;
				}
				if(Connected_users[UID_to_idx].protocol == TCP_PROT)
				{
					send(Users[UID_to_idx].Socket, message, MAX_LENGTH, 0);	
					send(Users[UID_to_idx].Socket, "priv", sizeof("priv"), 0);
				}
				else if(Connected_users[UID_to_idx].protocol == UDP_PROT)
				{
					sendto(Users[UID_to_idx].Socket, message, sizeof(message), 0, 
								(struct sockaddr*)&Users[UID_to_idx].info, 
								sizeof(Users[UID_to_idx].info));
					sendto(Users[UID_to_idx].Socket, "priv", sizeof("priv"), 0, 
								(struct sockaddr*)&Users[UID_to_idx].info, 
								sizeof(Users[UID_to_idx].info));
				}
			}		
			pthread_mutex_unlock(&arr_mutex);
		}
		//if message read error
		else
		{
			break;
		}
	}


	pthread_mutex_lock(&cusers_mutex);
		Connected_users[ClientIdx].UID = 0;
		Connected_users[ClientIdx].protocol = UNDEF_PROT;
	pthread_mutex_unlock(&cusers_mutex);

	pthread_mutex_lock(&arr_mutex);
		Users[ClientIdx].UID = 0;
		Users[ClientIdx].Socket = 0;
		memset(&Users[ClientIdx].nickname, 0, sizeof(Users[ClientIdx].nickname));
		memset(&Users[ClientIdx].info, 0, sizeof(Users[ClientIdx].info));
	pthread_mutex_unlock(&arr_mutex);

	shutdown(SockFD, SHUT_RDWR);
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

void *UDPThrd(void *sock)
{
	int ConnectionFD;
	int msock;
	int * arg;
	int i_idx = 0;
	int UID_from = 0;
	int UID_to = 0;
	int UID_to_idx = 0;
	char msg_for_send[MAX_LENGTH];
	char message[MAX_LENGTH];
	char UID[10];
	pthread_t mythread;
	struct sockaddr_in GetInfo;
	msock = *((int *) sock);
	free(sock);
	pthread_detach(pthread_self());
	while(1)
	{
		int USERID = 0;
		ssize_t recvln = 0;
		memset(&GetInfo, 0, sizeof(GetInfo));
		memset(&message, 0, sizeof(message));
		memset(&UID, 0, sizeof(UID));
		socklen_t InfoLen = sizeof(GetInfo);
		recvln = recvfrom(msock, (void*)message, MAX_LENGTH, 0, (struct sockaddr*)&GetInfo, &InfoLen);
		if(recvln == -1) continue;	
		int comp = 0;
		//Find user
		//check for the existence of the user
		for(i_idx = 0; i_idx < MAX_CLIENTS; i_idx++)
		{
			if(Compare2SockAddr(&GetInfo, &Users[i_idx].info) == 0)
			{
				USERID = i_idx;
				comp = 1; //if user was found
				break;	
			}
		}
		//if isn't exist	
		if(comp == 0)
		{
			//Find first empty slot for user
			pthread_mutex_lock(&arr_mutex);
			for(i_idx = 0; i_idx < MAX_CLIENTS; i_idx++)
			{
				if(Users[i_idx].UID == 0)
				{
					Users[i_idx].UID = i_idx+1;
					Users[i_idx].Socket = msock;
					strcpy(Users[i_idx].nickname, message);
					Users[i_idx].info = GetInfo;
					USERID = i_idx;
					break;	
				}
			}
			pthread_mutex_unlock(&arr_mutex);
			
			pthread_mutex_lock(&cusers_mutex);
			Connected_users[USERID].UID = Users[i_idx].UID;
			Connected_users[USERID].protocol = UDP_PROT;
			pthread_mutex_unlock(&cusers_mutex);
	
			//Send to USER his UID
			itoa(Users[USERID].UID, UID, 10);
			sendto(msock, UID, strlen(UID), 0, (struct sockaddr*)&GetInfo, sizeof(GetInfo));
			memset(&message, 0, sizeof(message));
			continue;
		} 
		if(strlen(message) > 0)
		{
			if(strcmp(message, "exit") == 0)
			{
				pthread_mutex_lock(&cusers_mutex);
					Connected_users[USERID].UID = 0;
					Connected_users[USERID].protocol = UNDEF_PROT;
				pthread_mutex_unlock(&cusers_mutex);
				
				pthread_mutex_lock(&arr_mutex);
					Users[USERID].UID = 0;
					Users[USERID].Socket = 0;
					memset(&Users[USERID].nickname, 0, sizeof(Users[USERID].nickname));
					memset(&Users[USERID].info, 0, sizeof(Users[USERID].info));
				pthread_mutex_unlock(&arr_mutex);

			continue;
				
			}

			parser(&message, &UID_from, &UID_to, &msg_for_send);
			memset(&message, 0, sizeof(message));
	
			pthread_mutex_lock(&arr_mutex);
			int ready1 = 0;
			int ready2 = 0;
			for(i_idx = 0; i_idx < MAX_CLIENTS; i_idx++)
			{
				//Find user which sent message
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
				//Find user who will receive the message
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
					if(Connected_users[i_idx].UID != UID_to)
					{
						if(Connected_users[i_idx].protocol == TCP_PROT)
						{
							send(Users[i_idx].Socket, message, MAX_LENGTH, 0);
							send(Users[i_idx].Socket, "publ", sizeof("publ"), 0);
						}
						else if(Connected_users[i_idx].protocol == UDP_PROT)
						{
							sendto(Users[i_idx].Socket, message, strlen(message), 0, 
								(struct sockaddr*)&Users[i_idx].info, 
								sizeof(Users[i_idx].info));
							sendto(Users[i_idx].Socket, "publ", sizeof("publ"), 0, 
								(struct sockaddr*)&Users[i_idx].info, 
								sizeof(Users[i_idx].info));
						}
					}
				}
			}
			//if message for single user
			else
			{
				int isExist = 0;
				for(i_idx = 0; i_idx < MAX_CLIENTS; i_idx++)
				{
					if(Users[i_idx].UID == UID_to)
					{
						isExist = 1;
						break;
					}
				}
				if(isExist == 0)
				{
					if(Connected_users[USERID].protocol == TCP_PROT)
					{
						send(Users[USERID].Socket, "The UID isn't exist", MAX_LENGTH, 0);	
					}	
					else if(Connected_users[USERID].protocol == UDP_PROT)
					{
						sendto(Users[USERID].Socket, "The UID isn't exist", strlen(message), 0, 
								(struct sockaddr*)&Users[USERID].info, 
								sizeof(Users[USERID].info));
					}
					continue;
				}
				if(Connected_users[UID_to_idx].protocol == TCP_PROT)
				{
					send(Users[UID_to_idx].Socket, message, MAX_LENGTH, 0);	
					send(Users[UID_to_idx].Socket, "priv", sizeof("priv"), 0);
				}
				else if(Connected_users[UID_to_idx].protocol == UDP_PROT)
				{
					sendto(Users[UID_to_idx].Socket, message, sizeof(message), 0, 
								(struct sockaddr*)&Users[UID_to_idx].info, 
								sizeof(Users[UID_to_idx].info));
					sendto(Users[UID_to_idx].Socket, "priv", sizeof("priv"), 0, 
								(struct sockaddr*)&Users[UID_to_idx].info, 
								sizeof(Users[UID_to_idx].info));
				}
			}		
			pthread_mutex_unlock(&arr_mutex);
		}
		//if the user just close his chat then we need to unload the information from the array of the users
		//it does not work at present
		else
		{
			pthread_mutex_lock(&cusers_mutex);
				Connected_users[USERID].UID = 0;
				Connected_users[USERID].protocol = UNDEF_PROT;
			pthread_mutex_unlock(&cusers_mutex);
			
			pthread_mutex_lock(&arr_mutex);
				Users[USERID].UID = 0;
				Users[USERID].Socket = 0;
				memset(&Users[USERID].nickname, 0, sizeof(Users[USERID].nickname));
				memset(&Users[USERID].info, 0, sizeof(Users[USERID].info));
			pthread_mutex_unlock(&arr_mutex);
		}
			
		
	}

	return NULL;
}

int main()
{
	//Variables
	int i_idx;
	int * TCParg;
	int * UDParg;
	int TCPSocketFD;
	int UDPSocketFD;
	struct sockaddr_in TCPServer;
	struct sockaddr_in UDPServer;
	struct tm * timeinfo;
	char File_name[60];
	char option[50];
	time_t rawtime;
	//Threads
	pthread_t TCPthread;
	pthread_t UDPthread; 
	//ArrMutex
	if(pthread_mutex_init(&arr_mutex, 0) != 0)
	{
		perror("\nArray mutex error:init");
		log = NULL;
		shutdown(TCPSocketFD, SHUT_RDWR);
		close(TCPSocketFD);
		exit(EXIT_FAILURE);
	}
	//log mutex
	if(pthread_mutex_init(&log_mutex, 0) != 0)
	{
		perror("\nLog mutex error:init");
		log = NULL;
		shutdown(TCPSocketFD, SHUT_RDWR);
		close(TCPSocketFD);
		exit(EXIT_FAILURE);
	}
	//connected users mutex
	if(pthread_mutex_init(&cusers_mutex, 0) != 0)
	{
		perror("\nConnected users mutex error:init");
		log = NULL;
		shutdown(TCPSocketFD, SHUT_RDWR);
		close(TCPSocketFD);
		exit(EXIT_FAILURE);
	}
	//Create TCP scoket
	if((TCPSocketFD = socket(PF_INET, SOCK_STREAM, 0)) == -1)
	{
		perror("\nTCP Error:create");
		log = NULL;
		shutdown(TCPSocketFD, SHUT_RDWR);
		close(TCPSocketFD);
		exit(EXIT_FAILURE);
	}
	printf("\nTCP Socket has been created");
	//Create UDP socket
	if((UDPSocketFD = socket(PF_INET, SOCK_DGRAM, 0)) == -1)
	{
		perror("\nUDP Error:create");
		log = NULL;
		shutdown(UDPSocketFD, SHUT_RDWR);
		shutdown(TCPSocketFD, SHUT_RDWR);
		close(TCPSocketFD);
		close(UDPSocketFD);
		exit(EXIT_FAILURE);
	}
	printf("\nUDP Socket has been created");
	//Clean up
	memset(&(*Users), 0, sizeof(*(Users)));
	for (i_idx = 0; i_idx < MAX_CLIENTS; i_idx++)
		memset(&Users[i_idx].info, 0, sizeof(Users[i_idx].info));
	memset(&(*Connected_users), 0, sizeof(*(Connected_users)));
	memset(&TCPServer, 0, sizeof(TCPServer));
	memset(&UDPServer, 0, sizeof(UDPServer));
	//initialize
	TCPServer.sin_family = PF_INET;
	TCPServer.sin_port = htons(PORT);
	TCPServer.sin_addr.s_addr = htonl(INADDR_ANY);
	UDPServer.sin_family = PF_INET;
	UDPServer.sin_port = htons(PORT);
	UDPServer.sin_addr.s_addr = htonl(INADDR_ANY);
	//Bind TCP socket
	if(bind(TCPSocketFD, Sadr &TCPServer, sizeof(TCPServer)) == -1)
	{
		perror("\nTCP Error:bind");
		log = NULL;
		shutdown(UDPSocketFD, SHUT_RDWR);
		shutdown(TCPSocketFD, SHUT_RDWR);
		close(TCPSocketFD);
		close(UDPSocketFD);
		exit(EXIT_FAILURE);	
	}
	printf("\nTCP Socket has been bound");
	//Bind UDP socket
	if(bind(UDPSocketFD, Sadr &UDPServer, sizeof(UDPServer)) == -1)
	{
		perror("\nUDP Error:bind");
		log = NULL;
		shutdown(UDPSocketFD, SHUT_RDWR);
		shutdown(TCPSocketFD, SHUT_RDWR);
		close(TCPSocketFD);
		close(UDPSocketFD);
		exit(EXIT_FAILURE);
	}
	printf("\nUDP Socket has been bound");
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
		shutdown(UDPSocketFD, SHUT_RDWR);
		shutdown(TCPSocketFD, SHUT_RDWR);
		close(TCPSocketFD);
		close(UDPSocketFD);
		exit(EXIT_FAILURE);
	}
	printf("\nLog opened");
	//Create TCP Thread
	TCParg = malloc(sizeof(int));
	*TCParg = TCPSocketFD;
	pthread_create(&TCPthread, 0, TCPThrd, TCParg);
	//Create UDP Thread
	UDParg = malloc(sizeof(int));
	*UDParg = UDPSocketFD;
	pthread_create(&UDPthread, 0, UDPThrd, UDParg);
	//listen socket
	if(listen(TCPSocketFD, SOMAXCONN) == -1)
	{
		perror("\nTCP Error:listen");
		shutdown(UDPSocketFD, SHUT_RDWR);
		shutdown(TCPSocketFD, SHUT_RDWR);
		close(TCPSocketFD);
		close(UDPSocketFD);
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
	pthread_mutex_destroy(&cusers_mutex);
	pthread_mutex_destroy(&arr_mutex);
	shutdown(UDPSocketFD, SHUT_RDWR);
	shutdown(TCPSocketFD, SHUT_RDWR);
	close(UDPSocketFD);
	close(TCPSocketFD);
	if(fclose(log) != 0)
		perror("\nFile error:close\n");
	else printf("\nLog closed\n");
	log = NULL;
	return EXIT_SUCCESS;	
}

