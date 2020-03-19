#ifndef PTHREADPOOL_H
#define PTHREADPOOL_H

#include <list>
#include <stdio.h>
#include <pthread.h>
#include <exception>
#include "locker.h"
//#include "chat.h"

template<typename T>
class ThreadPool
{
public:
	//线程池中线程数量，请求队列中最多待处理的请求的数量
	ThreadPool(int thread_number = 8,int max_requests = 10000);
	~ThreadPool();
	//向请求队列中添加任务
	bool append(T* request);
private:
	//工作线程的运行函数
	static void* worker(void* arg);    //c++中pthread_create的第三个参数必须指向静态函数
	void run();

	int m_thread_number;          //线程池中线程数
	int m_max_requests;           //请求队列中最大待处理请求数
	pthread_t* m_threads;         //线程数组，大小为m_thread_number,存放池中线程号
	std::list<T*>m_workqueue;     //请求队列
	Mutex m_queuelocker;           //保护队列的互斥锁
	sem m_queuestat;              //是否有待处理请求，信号量
	bool m_stop;                  //是否结束线程
};

template<typename T>
ThreadPool<T>::ThreadPool(int thread_number,int max_requests) :
	m_thread_number(thread_number),m_max_requests(max_requests),m_stop(false),m_threads(NULL)
{
	if(thread_number <= 0 || max_requests <= 0)
		throw std::exception();
	m_threads = new pthread_t[m_thread_number];
	if(!m_threads)
		throw std::exception();
	
	for(int i = 0;i < thread_number;i++){
		//printf("create the %dth thread\n",i+1);
		if(pthread_create(m_threads+i,NULL,worker,this) != 0) {
			delete [] m_threads;
			throw std::exception();
		}
		if(pthread_detach(m_threads[i]) != 0) {
			delete [] m_threads;
			throw std::exception();
		}
	}
}

template<typename T>
ThreadPool<T>::~ThreadPool()
{
	delete [] m_threads;
	m_stop = true;
}

template<typename T>
bool ThreadPool<T>::append(T* request)            //向工作队列添加任务
{
	m_queuelocker.lock();
	if(m_workqueue.size() > m_max_requests){
		m_queuelocker.unlock();
		return false;
	}
	m_workqueue.push_back(request);
	m_queuelocker.unlock();
	m_queuestat.post();             //信号量+1
	//printf("append into workqueue,sem++\n");
	return true;
}

template<typename T>
void* ThreadPool<T>::worker(void* arg)
{
	ThreadPool* pool = (ThreadPool*) arg;          //通过this指针的方式工作线程访问类的动态成员
	pool -> run();
	return pool;
}

template<typename T>
void ThreadPool<T>::run()
{
	while(!m_stop) {
		m_queuestat.wait();
		m_queuelocker.lock();
		if(m_workqueue.empty()) {
			m_queuelocker.unlock();
			continue;
		}
		T* request = m_workqueue.front();
		m_workqueue.pop_front();
		m_queuelocker.unlock();
		if(!request)
			continue;
		//printf("one of threads is work,do the process()\n");
		request -> process();                    //process()定义在数据结构T中，由线程池中工作线程调用，处理请求
	}
}
#endif