#ifndef THREADPOOL_H
#define THREADPOOL_H

#include<list>
#include<stdio.h>
#include<thread>
#include<vector>
#include<stdexcept>

#include "../lock/locker.h"
#include "../CGImysql/sql_connection_pool.h"



template<typename T>
class threadpool
{
public:
	threadpool(connection_pool* connpool,int thread_number = 8,int max_requests = 10000);
	~threadpool();
    
    threadpool(const threadpool&) = delete;
    threadpool& operator=(const threadpool&) = delete;

	bool append(T* request);

private:
	void run();
private:
	int m_thread_number;
	int m_max_requests;
    
    std::vector<std::thread> m_threads;

	std::list<T*> m_workqueue;
	locker m_mutex;
	sem m_queuestat;
	bool m_stop;
	connection_pool* m_connpool;

};

template<typename T>
threadpool<T>::threadpool(connection_pool* connpool,int thread_number,int max_requests):
	m_thread_number(thread_number),m_max_requests(max_requests),m_stop(false),m_connpool(connpool)
{
	if(thread_number <= 0 || max_requests <= 0)
	{
		throw std::invalid_argument("thread_number and max_requests must be positive");
	}

	m_threads.reserve(m_thread_number);

    try
    {
        for(int i = 0; i < m_thread_number; ++i)
        {
            m_threads.emplace_back(
                &threadpool<T>::run, this
            );
        }
    }
    catch(...)
    {
        m_mutex.lock();
        m_stop = true;
        m_mutex.unlock();
        
        for(std::size_t i=0; i < m_threads.size(); ++i)
        {
            m_queuestat.post();
        }
        
        for(auto& thread:m_threads)
        {
            if(thread.joinable())
            {
                thread.join();
            }
        }
        
        throw;
    }

}

template<typename T>
threadpool<T>::~threadpool()
{
	m_mutex.lock();
	m_stop = true;
    m_mutex.unlock();

    for(std::size_t i=0; i < m_threads.size(); ++i)
    {
        m_queuestat.post();
    }

    for(auto& thread:m_threads)
    {
        if(thread.joinable())
        {
            thread.join();
        }
    }
}

template<typename T>
bool threadpool<T>::append(T* request)
{
    if(!request)
    {
        return false;
    }

	m_mutex.lock();
	if(m_stop || m_workqueue.size() >= m_max_requests)
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
void threadpool<T>::run()
{
	while(true)
	{
	    if(!m_queuestat.wait())
        {
            continue;
        }
        m_mutex.lock();

        if(m_stop && m_workqueue.empty())
        {
            m_mutex.unlock();
            break;
        }
        
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
        
        connectionRAII mysqlcon(
            &request->m_mysql,
            m_connpool);
        request->process();
	}
}

#endif
