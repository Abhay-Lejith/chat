#include <pthread.h>
#include "bool.h"

typedef struct client_info
{
	int sockfd;
	char nickname[20];
	struct sockaddr_in address;
} client_info;

typedef struct list_entry
{
	struct client_info *client_info;
	pthread_mutex_t *mutex;
	struct list_entry *next;
} list_entry;

void llist_init(list_entry *list_start);
int llist_insert(list_entry *list_start, client_info *element);
int llist_remove_by_sockfd(list_entry *list_start, int sockfd);
list_entry* llist_find_by_sockfd(list_entry *list_start, int sockfd);
list_entry* llist_find_by_nickname(list_entry *list_start, char *nickname);
int llist_change_by_sockfd(list_entry *list_start, client_info *element, int sockfd);
int llist_show(list_entry *list_start);
int llist_get_count(list_entry *list_start);
int llist_get_nicknames(list_entry *list_start, char** nicks);
