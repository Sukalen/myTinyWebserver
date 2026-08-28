#ifndef BLOCK_QUEUE_H
#define BLOCK_QUEUE_H

#include<vector>
#include<mutex>
#include<condition_variable>
#include<chrono>
#include<stdexcept>
#include<cstddef>
#include<utility>



template<typename T>
class block_queue
{
public:
	explicit block_queue(std::size_t max_size = 1000)
        :m_array(max_size), m_size(0), m_max_size(max_size), m_front(0), m_back(0),m_closed(false)
	{
		if( 0 == max_size)
		{
			throw std::invalid_argument("block_queue max_size must be positive");
		}

	}
	
	~block_queue() = default;

    block_queue(const block_queue&) = delete;
    block_queue& operator=(const block_queue&) = delete;
	
	void clear()
	{
        std::lock_guard<std::mutex> lock(m_mutex);
		m_size = 0;
		m_front = 0;
		m_back = 0;
	}

	bool empty() const
	{
	    std::lock_guard<std::mutex> lock(m_mutex);
        return 0 == m_size;
	}

	bool full() const
	{
	    std::lock_guard<std::mutex> lock(m_mutex);
		return m_size >= m_max_size;
	}

	bool front(T& value) const
	{
	    std::lock_guard<std::mutex> lock(m_mutex);
		if(0 == m_size)
		{
			return false;
		}
		value = m_array[m_front];
		return true;
	}

	bool back(T& value) const
	{
	    std::lock_guard<std::mutex> lock(m_mutex);
		if(0 == m_size)
		{
			return false;
		}
        std::size_t last = (m_back + m_max_size - 1)% m_max_size;
		value = m_array[last];
		return true;
	}

	int size() const
	{
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_size;
	}

	int max_size() const
	{
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_max_size;
	}

	bool push(const T& item)
	{
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            
            
            if(m_closed || m_size >= m_max_size)
            {
                return false;
            }
            
            m_array[m_back] = item;
            m_back = (m_back + 1) % m_max_size;
            ++m_size;
        
        }

        m_cond.notify_one();

		return true;
	}

	bool pop(T& item)
	{
        std::unique_lock<std::mutex> lock(m_mutex);

        m_cond.wait(lock,[this]{ return m_closed || m_size > 0;});

        if( 0 == m_size)
        {
            return false;
        }
        
        item = std::move(m_array[m_front]);
        
        m_front = (m_front + 1) % m_max_size;
        --m_size;

		return true;
	}

	bool pop(T& item,int timeout_ms)
	{
        std::unique_lock<std::mutex> lock(m_mutex);

        bool ready = m_cond.wait_for(
                lock,
                std::chrono::milliseconds(timeout_ms),
                [this]{ return m_closed || m_size > 0;});
        if( !ready || 0 == m_size)
        {
            return false;
        }

        item = std::move(m_array[m_front]);
		m_front = (m_front + 1) % m_max_size;
		--m_size;
		return true;
	}	

    void close()
    {
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            m_closed = true;
        }
        m_cond.notify_all();
    }
private:
    std::vector<T> m_array;
    
    std::size_t m_size;
    std::size_t m_max_size;
    std::size_t m_front;
    std::size_t m_back;
    
    bool m_closed;

    mutable std::mutex m_mutex;
    std::condition_variable m_cond;

};

#endif
