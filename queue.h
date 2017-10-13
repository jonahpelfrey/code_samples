#ifndef __wqueue_h__
#define __wqueue_h__

#include <pthread.h>
#include <list>
#include <unistd.h>
#include <sys/time.h>
#include <time.h>
#include <errno.h>
#include <fstream>
#include <iostream>


template <typename T> class MessageQueue
{
	std::list<T>		m_queue;
	pthread_mutex_t 	m_mutex;
	pthread_cond_t 		m_empty, m_fill;
	pthread_cond_t  	not_empty, not_full;
	unsigned int		MAX;
	int 				buffer;

	public:
		MessageQueue()
		{
			pthread_mutex_init(&m_mutex, NULL);
			pthread_cond_init(&not_empty, NULL);
			pthread_cond_init(&not_full, NULL);
			pthread_cond_init(&m_empty, NULL);
			pthread_cond_init(&m_fill, NULL);
			MAX = 2000;
			buffer = 0;
		}
		
		~MessageQueue()
		{
			pthread_mutex_destroy(&m_mutex);
			pthread_cond_destroy(&not_empty);
			pthread_cond_destroy(&not_full);
		}

		T remove()
		{
			pthread_mutex_lock(&m_mutex);
			while( 0 == m_queue.size() )
			{
				pthread_cond_wait(&not_empty, &m_mutex);	
			}
			T item;
			item = m_queue.front();
			m_queue.pop_front();

			pthread_cond_signal(&not_full);
			pthread_mutex_unlock(&m_mutex);

			return item;
		}

		void add(T item)
		{
			pthread_mutex_lock(&m_mutex);
			while( MAX == m_queue.size() )
			{
				pthread_cond_wait(&not_full, &m_mutex);
			}
			m_queue.push_back(item);
			pthread_cond_signal(&not_empty);
			pthread_mutex_unlock(&m_mutex);
		}

		void exportContainer(MessageQueue<T>* temp)
		{
			pthread_mutex_lock(&m_mutex);
			temp->m_queue.splice(temp->m_queue.end(), m_queue, m_queue.begin(), m_queue.end());
			pthread_mutex_unlock(&m_mutex);
		}

		void importContainer(MessageQueue<T>* temp)
		{
			pthread_mutex_lock(&m_mutex);
			m_queue.splice(m_queue.end(), temp->m_queue, temp->m_queue.begin(), temp->m_queue.end());
			pthread_mutex_unlock(&m_mutex);
		}

		int size()
		{
			pthread_mutex_lock(&m_mutex);
			int size = m_queue.size();
			pthread_mutex_unlock(&m_mutex);

			return size;
		}
};

#endif