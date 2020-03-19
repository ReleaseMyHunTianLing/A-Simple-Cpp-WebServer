#ifndef LOCK_H
#define LOCK_H
#include <pthread.h>
#include <semaphore.h>
#include <exception>


class Mutex
{
	public:
		Mutex()
		{
			if(pthread_mutex_init(&m_mutex,NULL) != 0)
				throw std::exception();
		}
		~Mutex()
		{
			pthread_mutex_destroy(&m_mutex);
		}
		bool lock()
		{
			return pthread_mutex_lock(&m_mutex) == 0;
		}
		bool unlock()
		{
			return pthread_mutex_unlock(&m_mutex) == 0;
		}
	private:
		pthread_mutex_t m_mutex;
};

class sem
{
	public:
		sem()
		{
			if(sem_init(&m_sem,0,0) != 0)
				throw std::exception();
		}
		~sem()
		{
			sem_destroy(&m_sem);
		}
		bool wait()
		{
			return sem_wait(&m_sem) == 0;
		}
		bool post()
		{
			return sem_post(&m_sem) == 0;
		}
	private:
		sem_t m_sem;
};

#endif