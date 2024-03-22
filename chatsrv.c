#include <sys/types.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <stdlib.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <errno.h>
#include <signal.h>
#include <regex.h>
#include <getopt.h>
#include <limits.h>
#include "log.h"
#include "llist2.h"
#include "colors.h"

#define TRUE            1
#define FALSE           0

#define MAX_THREADS     1000      



typedef struct 
{
	char *ip;
	int port;
	int help;
	int loglevel;
	int version;
} cmd_params;



struct sockaddr_in server_address;
int server_sockfd;
int server_len, client_len;
cmd_params *params;
struct list_entry list_start;
int curr_thread_count = 0;
pthread_mutex_t curr_thread_count_mutex = PTHREAD_MUTEX_INITIALIZER;



int startup_server(void);
int parse_cmd_args(int *argc, char *argv[]);
void proc_client(int *arg);
void process_msg(char *message, int self_sockfd);
void send_welcome_msg(int sockfd);
void send_broadcast_msg(char* format, ...);
void send_private_msg(char* nickname, char* format, ...);
void chomp(char *s);
void change_nickname(char *oldnickname, char *newnickname);
void shutdown_server(int sig);
int get_client_info_idx_by_sockfd(int sockfd);
int get_client_info_idx_by_nickname(char *nickname);



int main(int argc, char *argv[])
{
	int i = 0;
	int ret = 0;
	struct sockaddr_in client_address;
	int client_sockfd = 0;
	pthread_t threads[MAX_THREADS];
			
	params = malloc(sizeof(cmd_params));
	ret = parse_cmd_args(&argc, argv);
	
	switch (params->loglevel)
	{
		case 1: set_log_level(LOG_ERROR); break;
		case 2: set_log_level(LOG_INFO); break;
		case 3: set_log_level(LOG_DEBUG); break;
		default: set_log_level(LOG_ERROR);
	}
	
	signal(SIGINT, shutdown_server);
	signal(SIGTERM, shutdown_server);
		
	if (startup_server() < 0)
	{
		disp(LOG_ERROR, "Error during server startup. Please consult debug log for details.");
		exit(-1);
	}
		
	disp(LOG_INFO, "Server listening on %s, port %d", params->ip, params->port);
	switch (params->loglevel)
	{
		case LOG_ERROR: disp(LOG_INFO, "Log level set to ERROR"); break;
		case LOG_INFO: disp(LOG_INFO, "Log level set to INFO"); break;
		case LOG_DEBUG: disp(LOG_INFO, "Log level set to DEBUG"); break;
		default: disp(LOG_INFO, "Unknown log level specified"); break;
	}
		
	while (1)
	{
		disp(LOG_INFO, "Waiting for incoming connection...");
			
		client_len = sizeof(client_address);
		client_sockfd = accept(server_sockfd, (struct sockaddr *)&client_address, (socklen_t *)&client_len);

		if (client_sockfd > 0)
		{
			disp(LOG_INFO, "Server accepted new connection on socket id %d", client_sockfd);
			
			/* A connection between a client and the server has been established.
			 * Now create a new thread and handover the client_sockfd
			 */
			pthread_mutex_lock(&curr_thread_count_mutex);
			if (curr_thread_count < MAX_THREADS)
			{			
				client_info *ci = (client_info *)malloc(sizeof(client_info));
				ci->sockfd = client_sockfd;
				ci->address = client_address;
				sprintf(ci->nickname, "anonymous_%d", client_sockfd);
								
				llist_insert(&list_start, ci);
				llist_show(&list_start);
									
				ret = pthread_create(&threads[curr_thread_count],
					NULL,
					(void *)&proc_client, 
					(void *)&client_sockfd); 
			
				if (ret == 0)
				{
					pthread_detach(threads[curr_thread_count]);
					curr_thread_count++;
															
					disp(LOG_INFO, "User %s joined the chat.", ci->nickname);	
					disp(LOG_DEBUG, "main(): Connections used: %d of %d", curr_thread_count, MAX_THREADS);
				}
				else
				{
					free(ci);
					close(client_sockfd);
				}
			}
			else
			{
				disp(LOG_ERROR, "Max. connections reached. Connection limit is %d. Connection dropped.", MAX_THREADS);
				close(client_sockfd);
			}
			pthread_mutex_unlock(&curr_thread_count_mutex);
		}
		else
		{			
			perror(strerror(errno));
			exit(-3);
		}
	}
	
	free(params);

	return 0;
}


int startup_server(void)
{
	int optval = 1;
		
	llist_init(&list_start);
		
	server_sockfd = socket(AF_INET, SOCK_STREAM, 0);
	if (setsockopt(server_sockfd, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof optval) != 0)
	{
		disp(LOG_DEBUG, "Error calling setsockopt(): %s", strerror(errno));
		return -1;
	}
	
	server_address.sin_family = AF_INET;
	server_address.sin_addr.s_addr = inet_addr(params->ip);
	server_address.sin_port = htons(params->port);
	server_len = sizeof(server_address);
	if (bind(server_sockfd, (struct sockaddr *)&server_address, server_len) != 0)
	{
		disp(LOG_DEBUG, "Error calling bind(): %s", strerror(errno));
		return -2;
	}
	
	if (listen(server_sockfd, 5) != 0)
	{
		disp(LOG_DEBUG, "Error calling listen(): %s", strerror(errno));
		return -3;
	}

	return 0;
}


int parse_cmd_args(int *argc, char *argv[])
{
	int option_index = 0;
	int c;
	
	params->ip = "127.0.0.1";
	params->port = 5555;
	params->help = 0;
	params->loglevel = LOG_INFO;
	params->version = 0;

	static struct option long_options[] = 
	{
		{ "ip",			required_argument, 0, 'i' },
		{ "port",		required_argument, 0, 'p' },
		{ "help",		no_argument,       0, 'h' },
		{ "version",	no_argument,       0, 'v' },
		{ "loglevel",	required_argument, 0, 'l' },
		{ 0, 0, 0, 0 }
	};

	while (1)
	{
		c = getopt_long(*argc, argv, "i:p:hvl:", long_options, &option_index);
		
		if (c == -1)
			break;

		switch (c)
		{
			case 'i': params->ip = optarg; break;
			case 'p': params->port = atoi(optarg); break;
			case 'h': params->help = 1; break;
			case 'v': params->version = 1; break;
			case 'l': params->loglevel = atoi(optarg); break;
		}
	}

	return 0;
}


void proc_client(int *arg)
{
	char buffer[1024];
	char message[1024];
	int ret = 0;
	int len = 0;
	int socklen = 0;
	struct list_entry *list_entry;
	fd_set readfds;
	int sockfd = 0;
		
	sockfd = *arg;
	list_entry = llist_find_by_sockfd(&list_start, sockfd);
	
	memset(message, 0, 1024);
	FD_ZERO(&readfds);
	FD_SET(sockfd, &readfds);
	
	send_welcome_msg(sockfd);
	
	send_broadcast_msg("%sUser %s joined the chat.%s\r\n", color_magenta, list_entry->client_info->nickname, color_normal);

	while (1)
	{
		ret = select(FD_SETSIZE, &readfds, (fd_set *)0, 
			(fd_set *)0, (struct timeval *) 0);
		if (ret == -1)
		{
			disp(LOG_ERROR, "Error calling select() on thread.");
			perror(strerror(errno));
		}
		else
		{			
			memset(buffer, 0, 1024);
			socklen = sizeof(list_entry->client_info->address);
			len = recvfrom(sockfd, buffer, sizeof(buffer), 0, 
				(struct sockaddr *)&list_entry->client_info->address, (socklen_t *)&socklen);

			disp(LOG_DEBUG, "proc_client(): Receive buffer contents = %s", buffer);
			
			if (sizeof(message) - strlen(message) > strlen(buffer))
			{
				strcat(message, buffer);
			}
			
			char *pos = strstr(message, "\n");
			if (pos != NULL)
			{		  		
				chomp(message);
				disp(LOG_DEBUG, "proc_client(): Message buffer contents = %s", message);
				disp(LOG_DEBUG, "proc_client(): Complete message received.");
		  		
		  		process_msg(message, sockfd);
		  		memset(message, 0, 1024);	
			}
			else
			{
				disp(LOG_DEBUG, "proc_client(): Message buffer contents = %s", message);
				disp(LOG_DEBUG, "proc_client(): Message still incomplete.");
			}
		}
	}
}

void process_msg(char *message, int self_sockfd)
{
	char buffer[1024];
	regex_t regex_quit;
	regex_t regex_nick;
	regex_t regex_msg;
	int ret;
	char newnick[20];
	char oldnick[20];
	char priv_nick[20];
	struct list_entry *list_entry = NULL;
	struct list_entry *nick_list_entry = NULL;
	struct list_entry *priv_list_entry = NULL;
	int processed = FALSE;
	size_t ngroups = 0;
	size_t len = 0;
	regmatch_t groups[3];
	
	memset(buffer, 0, 1024);
	memset(newnick, 0, 20);
	memset(oldnick, 0, 20);
	memset(priv_nick, 0, 20);
		
	list_entry = llist_find_by_sockfd(&list_start, self_sockfd);
		
	chomp(message);
	
	regcomp(&regex_quit, "^/quit$", REG_EXTENDED);
	regcomp(&regex_nick, "^/nick ([a-zA-Z0-9_]{1,19})$", REG_EXTENDED);
	regcomp(&regex_msg, "^/msg ([a-zA-Z0-9_]{1,19}) (.*)$", REG_EXTENDED);

	ret = regexec(&regex_quit, message, 0, NULL, 0);
	if (ret == 0)
	{		
		send_broadcast_msg("%sUser %s has left the chat server.%s\r\n", 
			color_magenta, list_entry->client_info->nickname, color_normal);
		disp(LOG_INFO, "User %s has left the chat server.", list_entry->client_info->nickname);
		pthread_mutex_lock(&curr_thread_count_mutex);
		curr_thread_count--;
		disp(LOG_DEBUG, "process_msg(): Connections used: %d of %d", curr_thread_count, MAX_THREADS);
		pthread_mutex_unlock(&curr_thread_count_mutex);
		
		disp(LOG_DEBUG, "process_msg(): Removing element with sockfd = %d", self_sockfd);
		llist_remove_by_sockfd(&list_start, self_sockfd);
		
		close(self_sockfd);
		
		regfree(&regex_quit);
		regfree(&regex_nick);
		regfree(&regex_msg);
				
		pthread_exit(0);
	}
			
	ngroups = 2;
	ret = regexec(&regex_nick, message, ngroups, groups, 0);
	if (ret == 0)
	{
		processed = TRUE;
				
		len = groups[1].rm_eo - groups[1].rm_so;
		strncpy(newnick, message + groups[1].rm_so, len);
		strcpy(oldnick, list_entry->client_info->nickname);
		
		strcpy(buffer, "User ");
		strcat(buffer, oldnick);
		strcat(buffer, " is now known as ");
		strcat(buffer, newnick);
									
		nick_list_entry = llist_find_by_nickname(&list_start, newnick);
		if (nick_list_entry == NULL)
		{
			change_nickname(oldnick, newnick);
			send_broadcast_msg("%s%s%s\r\n", color_yellow, buffer, color_normal);
			disp(LOG_INFO, buffer);
		}
		else
		{
			send_private_msg(oldnick, "%sCHATSRV: Cannot change nickname. Nickname already in use.%s\r\n", 
				color_yellow, color_normal);
			disp(LOG_INFO, "Private message from CHATSRV to %s: Cannot change nickname. Nickname already in use", 
				oldnick);
		}
	}
		
	ngroups = 3;
	ret = regexec(&regex_msg, message, ngroups, groups, 0);
	if (ret == 0)
	{
		processed = TRUE;
				
		len = groups[1].rm_eo - groups[1].rm_so;
		memcpy(priv_nick, message + groups[1].rm_so, len);
		len = groups[2].rm_eo - groups[2].rm_so;
		memcpy(buffer, message + groups[2].rm_so, len);
		
		priv_list_entry = llist_find_by_nickname(&list_start, priv_nick);
		if (priv_list_entry != NULL)
		{
			send_private_msg(priv_nick, "%s%s:%s %s%s%s\r\n", color_green, list_entry->client_info->nickname, 
				color_normal, color_red, buffer, color_normal);
			disp(LOG_INFO, "Private message from %s to %s: %s", 
				list_entry->client_info->nickname, priv_nick, buffer);
		}
	}
		
	if (processed == FALSE)
	{
		send_broadcast_msg("%s%s:%s %s\r\n", color_green, list_entry->client_info->nickname, color_normal, message);
		disp(LOG_INFO, "%s: %s", list_entry->client_info->nickname, message);
	}

	llist_show(&list_start);
		
	regfree(&regex_quit);
	regfree(&regex_nick);
	regfree(&regex_msg);

}

void send_welcome_msg(int sockfd)
{
	int socklen = 0;
	struct list_entry *cur = NULL;
	va_list args;
	char buffer[1024];
		
	cur = llist_find_by_sockfd(&list_start, sockfd);
	socklen = sizeof(cur->client_info->address);
	
	pthread_mutex_lock(cur->mutex);
	
	memset(buffer, 0, 1024);
	sprintf(buffer, "%s|  Connected to Server  |%s\r\n", color_white, color_normal);
	sendto(cur->client_info->sockfd, buffer, strlen(buffer), 0,
		(struct sockaddr *)&(cur->client_info->address), (socklen_t)socklen);	
	pthread_mutex_unlock(cur->mutex);
}


void send_broadcast_msg(char* format, ...)
{
	int socklen = 0;
	//client_info *cur = NULL;
	struct list_entry *cur = NULL;
	va_list args;
	char buffer[1024];
		
	memset(buffer, 0, 1024);
	
	va_start(args, format);
	vsprintf(buffer, format, args);
	va_end(args);
	
	cur = &list_start;
	while (cur != NULL)
	{	
		pthread_mutex_lock(cur->mutex);
				
		if (cur->client_info != NULL)
		{
			socklen = sizeof(cur->client_info->address);
			sendto(cur->client_info->sockfd, buffer, strlen(buffer), 0,
				(struct sockaddr *)&(cur->client_info->address), (socklen_t)socklen);
		}
				
		pthread_mutex_unlock(cur->mutex);
				
		cur = cur->next;
	}
}


void send_private_msg(char* nickname, char* format, ...)
{
	int socklen = 0;
	struct list_entry *cur = NULL;
	va_list args;
	char buffer[1024];
		
	memset(buffer, 0, 1024);
	
	va_start(args, format);
	vsprintf(buffer, format, args);
	va_end(args);
	
	cur = llist_find_by_nickname(&list_start, nickname);
	
	pthread_mutex_lock(cur->mutex);

	socklen = sizeof(cur->client_info->address);
	sendto(cur->client_info->sockfd, buffer, strlen(buffer), 0,
		(struct sockaddr *)&(cur->client_info->address), (socklen_t)socklen);
		
	pthread_mutex_unlock(cur->mutex);
}


void chomp(char *s) 
{
	while(*s && *s != '\n' && *s != '\r') s++;

	*s = 0;
}


void change_nickname(char *oldnickname, char *newnickname)
{
	struct list_entry *list_entry = NULL;
	client_info *ci_new = NULL;
	int idx = 0;
	
	disp(LOG_DEBUG, "change_nickname(): oldnickname = %s, newnickname = %s", oldnickname, newnickname);
		
	list_entry = llist_find_by_nickname(&list_start, oldnickname);
		
	pthread_mutex_lock(list_entry->mutex);
	
	disp(LOG_DEBUG, "change_nickname(): client_info found. client_info->nickname = %s", 
		list_entry->client_info->nickname);
	
	strcpy(list_entry->client_info->nickname, newnickname);
		
	pthread_mutex_unlock(list_entry->mutex);
}


void shutdown_server(int sig)
{
	list_entry *cur = NULL;
		
		disp(LOG_INFO, "Closing socket connections...");		
				
		cur = &list_start;
		while (cur != NULL)
		{			
			pthread_mutex_lock(cur->mutex);
					
			if (cur->client_info != NULL)
			{
				close(cur->client_info->sockfd);
			}
					
			pthread_mutex_unlock(cur->mutex);
					
			cur = cur->next;
		}		
				
		disp(LOG_INFO, "Shutting down listener...");
		close(server_sockfd);
				
		disp(LOG_INFO, "Exiting. Byebye.");
		exit(0);

}





