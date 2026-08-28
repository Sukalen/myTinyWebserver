#ifndef LOG_H
#define LOG_H

#include<cstdio>
#include<cstdarg>
#include<memory>
#include<mutex>
#include<iostream>
#include<string>
#include<thread>
#include<vector>

#include "block_queue.h"


#define LOG_DEBUG(format,...) Log::get_instance()->write_log(0,format,##__VA_ARGS__)
#define LOG_INFO(format,...) Log::get_instance()->write_log(1,format,##__VA_ARGS__)
#define LOG_WARN(format,...) Log::get_instance()->write_log(2,format,##__VA_ARGS__)
#define LOG_ERROR(format,...) Log::get_instance()->write_log(3,format,##__VA_ARGS__)


class Log
{
public:
	static Log* get_instance()
	{
		static Log instance;
		return &instance;
	}


	bool init(const char* file_name,int log_buf_size = 8192,int split_lines = 5000000,int max_queue_size = 0);

	void write_log(int level,const char* format,...);
	
	void flush();

    Log(const Log&) = delete;
    Log& operator=(const Log&) = delete;

private:
    struct FileCloser
    {
        void operator()(FILE* fp) const noexcept
        {
            if(fp)
            {
                std::fclose(fp);
            }
        }
    };
	Log() = default;
	virtual ~Log();
	void async_write_log();

private:
    std::string m_dir_name;
    std::string m_log_name;
	
    int m_split_lines = 0;
	int m_log_buf_size = 0;
	long long m_count = 0;
	int m_today = 0;
    
    std::unique_ptr<FILE,FileCloser> m_fp;
    
    std::vector<char> m_buf;

    std::unique_ptr<block_queue<std::string>> m_log_queue;

    std::thread m_write_thread;

    bool m_is_async = false;

    std::mutex m_mutex;
	
};


#endif
