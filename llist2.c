#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <netinet/in.h>
#include "llist2.h"
#include "log.h"

#define TRUE            1
#define FALSE           0


void llist_init(list_entry *list_start)
{
	list_start->client_info = NULL;
	list_start->next = NULL;
	list_start->mutex = (pthread_mutex_t *)malloc(sizeof(pthread_mutex_t));	
	pthread_mutex_init(list_start->mutex, NULL);
}

int llist_insert(list_entry *list_start, client_info *element)
{
	list_entry *cur, *prev;
	int inserted = FALSE;

	cur = prev = list_start;
	
	while (cur != NULL)
	{
		pthread_mutex_lock(cur->mutex);	
	
		if (cur->client_info == NULL)
		{
			cur->client_info = element;
			pthread_mutex_unlock(cur->mutex);
			inserted = TRUE;
			break;
		}
	
		pthread_mutex_unlock(cur->mutex);
	
		prev = cur;
		cur = cur->next;
	}
	
	if (inserted == FALSE)
	{

		pthread_mutex_lock(prev->mutex);
				
		list_entry *new_entry = (list_entry *)malloc(sizeof(list_entry));
		new_entry->client_info = element;
		new_entry->next = NULL;
		new_entry->mutex = (pthread_mutex_t *)malloc(sizeof(pthread_mutex_t));	
		pthread_mutex_init(new_entry->mutex, NULL);
			
		prev->next = new_entry;
				
		pthread_mutex_unlock(prev->mutex);
	
		inserted = TRUE;
	}

	if (inserted == TRUE)
	{
		return 0;
	}
	else
	{
		return -1;
	}
}

int llist_remove_by_sockfd(list_entry *list_start, int sockfd)
{
	list_entry *cur;

	cur = list_start;
	
	while (cur != NULL)
	{
		
		pthread_mutex_lock(cur->mutex);	
			
		if (cur->client_info != NULL)
		{
			
			if (cur->client_info->sockfd == sockfd)
			{
				cur->client_info = NULL;
				pthread_mutex_unlock(cur->mutex);
				break;
			}

		}
			
		pthread_mutex_unlock(cur->mutex);
			
		cur = cur->next;
	}

	return 0;
}


list_entry* llist_find_by_sockfd(list_entry *list_start, int sockfd)
{
	list_entry *cur;

	cur = list_start;
	
	while (cur != NULL)
	{		
		pthread_mutex_lock(cur->mutex);	
		
		if (cur->client_info != NULL)
		{
				
			if (cur->client_info->sockfd == sockfd)
			{
				pthread_mutex_unlock(cur->mutex);
				return cur;
			}

		}
	
		pthread_mutex_unlock(cur->mutex);
			
		cur = cur->next;
	}
	
	return NULL;
}


list_entry* llist_find_by_nickname(list_entry *list_start, char *nickname)
{
	list_entry *cur;

	cur = list_start;
	
	while (cur != NULL)
	{		
		pthread_mutex_lock(cur->mutex);	
		
		if (cur->client_info != NULL)
		{
				
			if (strcmp(cur->client_info->nickname, nickname) == 0)
			{
				pthread_mutex_unlock(cur->mutex);
				return cur;
			}

		}
			
		pthread_mutex_unlock(cur->mutex);
			
		cur = cur->next;
	}
	
	return NULL;
}


int llist_change_by_sockfd(list_entry *list_start, client_info *element, int sockfd)
{
	list_entry *cur;

	cur = list_start;
	
	while (cur != NULL)
	{		
		pthread_mutex_lock(cur->mutex);	
		
		if (cur->client_info != NULL)
		{
				
			if (cur->client_info->sockfd == sockfd)
			{
				cur->client_info = element;
				pthread_mutex_unlock(cur->mutex);
				return 0;
			}

		}
			
		pthread_mutex_unlock(cur->mutex);
			
		cur = cur->next;
	}

	return 0;
}


int llist_show(list_entry *list_start)
{
	list_entry *cur;

	cur = list_start;
	
	logline(LOG_DEBUG, "---------- Client List Dump Begin ----------");
	while (cur != NULL)
	{		
		pthread_mutex_lock(cur->mutex);
				
		if (cur->client_info != NULL)
		{
			logline(LOG_DEBUG, "sockfd = %d, nickname = %s", cur->client_info->sockfd, cur->client_info->nickname);
		}
				
		pthread_mutex_unlock(cur->mutex);
				
		cur = cur->next;
	}
	logline(LOG_DEBUG, "----------- Client List Dump End -----------");
	
	return 0;
}


int llist_get_count(list_entry *list_start)
{
	list_entry *cur;
	int count = 0;

	cur = list_start;
	
	while (cur != NULL)
	{
		
		pthread_mutex_lock(cur->mutex);		
				
		if (cur->client_info != NULL)
		{
			count++;
		}
				
		pthread_mutex_unlock(cur->mutex);
					
		cur = cur->next;
	}
	
	return count;
}

int llist_get_nicknames(list_entry *list_start, char **nicks)
{
	list_entry *cur;

	cur = list_start;

	int count = 0;
	
	while (cur != NULL)
	{		
		pthread_mutex_lock(cur->mutex);
				
		if (cur->client_info != NULL)
		{
			strncpy(nicks[count++], cur->client_info->nickname, 20);
		}
				
		pthread_mutex_unlock(cur->mutex);
				
		cur = cur->next;
	}
	
	return count;
}
