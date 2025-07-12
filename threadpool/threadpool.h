#ifndef THREADPOOL_H
#define THREADPOOL_H

#include<list>
#include<stdio.h>
#include<exception>
#include<pthread.h>

#include "../lock/locker.h"
#include "../CGImysql/sql_connection_pool.h"

using std::list;


template<typename T>
class threadpool
{
public:
	threadpool(connection_pool* connpool,int thread_number = 8,int max_requests = 10000);
	~threadpool();
	bool append(T* request);

private:
	static void* worker(void* arg);
	void run();
private:
	int m_thread_number;
	int m_max_requests;
	pthread_t* m_threads;
	list<T*> m_workqueue;
	locker m_mutex;
	sem m_queuestat;
	bool m_stop;
	connection_pool* m_connpool;

};

template<typename T>
threadpool<T>::threadpool(connection_pool* connpool,int thread_number,int max_requests):
	m_thread_number(thread_number),m_max_requests(max_requests),m_threads(NULL),m_stop(false),m_connpool(connpool)
{
	if(thread_number <= 0 || max_requests <= 0)
	{
		throw std::exception();
	}
	m_threads = new pthread_t[m_thread_number];

	if(!m_threads)
	{
		throw std::exception();
	}

	for(int i = 0;i < m_thread_number;++i)
	{
		if(pthread_create(m_threads + i,NULL,worker,this) != 0)
		{
			delete[] m_threads;
			throw std::exception();
		}

		if(pthread_detach(m_threads[i]))
		{
			delete[] m_threads;
			throw std::exception();
		}
	}
}

template<typename T>
threadpool<T>::~threadpool()
{
	delete[] m_threads;
	m_stop = true;
}

template<typename T>
bool threadpool<T>::append(T* request)
{
	m_mutex.lock();
	if(m_workqueue.size() > m_max_requests)
	{
		m_mutex.unlock();
		return false;
	}
	m_workqueue.push_back(request);
	m_mutex.unlock();
	m_queuestat.post();
	return true;
}

template<typename T>
void* threadpool<T>::worker(void* arg)
{
	threadpool* pool = (threadpool*)arg;
	pool->run();
	return pool;
}

template<typename T>
void threadpool<T>::run()
{
	while(!m_stop)
	{
		m_queuestat.wait();
		m_mutex.lock();
		if(m_workqueue.empty())
		{
			m_mutex.unlock();
			continue;
		}
		T* request = m_workqueue.front();
		m_workqueue.pop_front();
		m_mutex.unlock();
		if(!request)
		{
			continue;
		}

		connectionRAII mysqlcon(&request->mysql,m_connpool);
		request->process();
	}
}

#endif
