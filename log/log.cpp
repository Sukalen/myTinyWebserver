#include<cstring>
#include<ctime>
#include<sys/time.h>
#include<algorithm>

#include "log.h"


Log::~Log()
{
	if(m_log_queue)
    {
        m_log_queue->close();
    }

    if(m_write_thread.joinable())
    {
        m_write_thread.join();
    }

    flush();
}

bool Log::init(const char* file_name, int log_buf_size, int split_lines, int max_queue_size)
{
	if(nullptr == file_name || log_buf_size <= 0 || split_lines <= 0)
    {
        return false;
    }

	m_log_buf_size = log_buf_size;
	m_split_lines = split_lines;
    m_count = 0;

    m_buf.resize(m_log_buf_size);

    std::string file_path(file_name);
    std::size_t pos = file_path.find_last_of('/');

    if( pos == std::string::npos)
    {
        m_dir_name.clear();
        m_log_name = file_path;
    }
    else
    {
        m_dir_name = file_path.substr(0,pos+1);
        m_log_name = file_path.substr(pos+1);
    }

    time_t t = time(nullptr);

    struct tm my_tm;
    localtime_r(&t,&my_tm);

    m_today = my_tm.tm_mday;
    
    char log_full_name[512] = {0};

    std::snprintf(log_full_name,sizeof(log_full_name),"%s%d_%02d_%02d_%s",
            m_dir_name.c_str(),
            my_tm.tm_year+1900,
            my_tm.tm_mon+1,
            my_tm.tm_mday,
            m_log_name.c_str());

    FILE* fp = std::fopen(log_full_name,"a");

    if( nullptr == fp)
    {
        return false;
    }

    m_fp.reset(fp);

    if(max_queue_size > 0)
    {
        m_log_queue = std::make_unique<block_queue<std::string>>(
                static_cast<std::size_t>(max_queue_size));

        m_is_async = true;
        
        try
        {
            m_write_thread = std::thread(&Log::async_write_log,this);
        }
        catch(...)
        {
            m_is_async = false;
            m_log_queue.reset();
            return false;
        }
    }

    return true;
}

void Log::async_write_log()
{
    std::string single_log;

    while(m_log_queue && m_log_queue->pop(single_log))
    {
        std::lock_guard<std::mutex> lock(m_mutex);

        if(m_fp)
        {
            std::fputs(single_log.c_str(),m_fp.get());
        }
    }
}

void Log::write_log(int level,const char* format,...)
{
	struct timeval now = {0,0};
	gettimeofday(&now,nullptr);
	time_t t = now.tv_sec;
	
    struct tm my_tm;
	localtime_r(&t,&my_tm);

    const char* level_str = "[info]:";

	switch(level)
	{
		case 0:
			level_str = "[debug]:";
			break;
		
		case 1:
			level_str = "[info]:";
			break;
		
		case 2:
			level_str = "[warn]:";
			break;
		
		case 3:
			level_str = "[error]:";
			break;
			
		default:
			level_str = "[info]:";
			break;
		
	}

	va_list arg_list;
	va_start(arg_list,format);

    std::string log_str;

    {
        std::lock_guard<std::mutex> lock(m_mutex);

	    ++m_count;
	    if(m_today != my_tm.tm_mday || m_count % m_split_lines == 0)
	    {
		    char new_log[512] = {0};

		    if(m_today != my_tm.tm_mday)
		    {
                std::snprintf(new_log,sizeof(new_log),"%s%d_%02d_%02d_%s",
                    m_dir_name.c_str(),
                    my_tm.tm_year + 1900,
                    my_tm.tm_mon + 1,
                    my_tm.tm_mday,
                    m_log_name.c_str());

			    m_today = my_tm.tm_mday;
			    m_count = 0;
		    }
		    else
		    {
                std::snprintf(new_log,sizeof(new_log),"%s%d_%02d_%02d_%s.%lld",
                    m_dir_name.c_str(),
                    my_tm.tm_year + 1900,
                    my_tm.tm_mon + 1,
                    my_tm.tm_mday,
                    m_log_name.c_str(),
                    m_count / m_split_lines);
            }
		    
		    FILE* new_fp = std::fopen(new_log,"a");

            if(new_fp)
            {
                if(m_fp)
                {
                    std::fflush(m_fp.get());
                }

                m_fp.reset(new_fp);
            }
	    }

	    int n = std::snprintf(m_buf.data(),m_buf.size(),
                "%d-%02d-%02d %02d:%02d:%02d.%06ld %s ",
                my_tm.tm_year + 1900,
                my_tm.tm_mon + 1,
			    my_tm.tm_mday,
                my_tm.tm_hour,
                my_tm.tm_min,
                my_tm.tm_sec,
                now.tv_usec,
                level_str);
        if( n < 0)
        {
            va_end(arg_list);
            return;
        }

        std::size_t offset = std::min<std::size_t>(
                static_cast<std::size_t>(n),m_buf.size()-1);

	    int m = std::vsnprintf(
                m_buf.data()+offset,
                m_buf.size()-offset,
                format,
                arg_list);
        if( m < 0)
        {
            va_end(arg_list);
            return;
        }
        std::size_t length = std::min<std::size_t>(
                offset+static_cast<std::size_t>(m), m_buf.size()-1);
        if(length + 1 < m_buf.size())
        {
            m_buf[length++] = '\n';
            m_buf[length] = '\0';
        }

        log_str.assign(m_buf.data(),length);
    }

	va_end(arg_list);

	if(m_is_async && m_log_queue && m_log_queue->push(log_str))
	{
		return;
	}

	std::lock_guard<std::mutex> lock(m_mutex);
    if(m_fp)
    {
        std::fputs(log_str.c_str(),m_fp.get());
    }
	
}

void Log::flush()
{
	std::lock_guard<std::mutex> lock(m_mutex);

    if(m_fp)
    {
        std::fflush(m_fp.get());
    }
}


