#include<string.h>
#include<time.h>
#include<sys/time.h>
#include<stdarg.h>
#include<pthread.h>
#include "log.h"


Log::Log()
{
	m_count = 0;
	m_is_async = false;
}

Log::~Log()
{
	if(m_fp != NULL)
	{
		fclose(m_fp);
	}
	if(m_log_queue != NULL)
	{
		delete m_log_queue;
		m_log_queue = NULL;
	}
	if(m_buf != NULL)
	{
		delete[] m_buf;
		m_buf = NULL;
	}
}

bool Log::init(const char* file_name, int log_buf_size, int split_lines, int max_queue_size)
{
	if(max_queue_size > 0)
	{
		m_is_async = true;
		m_log_queue = new block_queue<string>(max_queue_size);
		pthread_t tid;
		pthread_create(&tid,NULL,flush_log_thread,NULL);
	}

	m_log_buf_size = log_buf_size;
	m_buf = new char[m_log_buf_size];
	memset(m_buf,'\0',m_log_buf_size);
	m_split_lines = split_lines;

	time_t t = time(NULL);

	struct tm* sys_tm = localtime(&t);
	struct tm my_tm = *sys_tm;

	const char* p = strrchr(file_name,'/');
	char log_full_name[512] = {0};

	if(NULL == p)
	{
		snprintf(log_full_name,sizeof(log_full_name)-1,"%d_%02d_%02d_%s",my_tm.tm_year+1900,my_tm.tm_mon+1,my_tm.tm_mday,file_name);
	}
	else
	{
		strcpy(log_name, p+1);
		strncpy(dir_name, file_name, p-file_name + 1);
		snprintf(log_full_name,sizeof(log_full_name)-1,"%s%d_%02d_%02d_%s",dir_name,my_tm.tm_year+1900,my_tm.tm_mon+1,my_tm.tm_mday,log_name);
	}

	m_today = my_tm.tm_mday;

	m_fp = fopen(log_full_name,"a");
	if( NULL == m_fp)
	{
		return false;
	}

	return true;
}


void Log::write_log(int level,const char* format,...)
{
	struct timeval now = {0,0};
	gettimeofday(&now,NULL);
	time_t t = now.tv_sec;
	struct tm* sys_tm = localtime(&t);
	struct tm my_tm = *sys_tm;
	char s[16] = {0};

	switch(level)
	{
		case 0:
		{
			strcpy(s,"[debug]:");
			break;
		}
		case 1:
		{
			strcpy(s,"[info]:");
			break;
		}
		case 2:
		{
			strcpy(s,"[warn]:");
			break;
		}
		case 3:
		{
			strcpy(s,"[error]:");
			break;
		}	
		default:
		{
			strcpy(s,"[info]:");
			break;
		}
	}

	m_mutex.lock();
	m_count++;
	if(m_today != my_tm.tm_mday || m_count % m_split_lines == 0)
	{
		char new_log[512] = {0};
		fflush(m_fp);
		fclose(m_fp);
		char tail[16] = {0};

		snprintf(tail,16,"%d_%02d_%02d_",my_tm.tm_year+1900,my_tm.tm_mon+1,my_tm.tm_mday);

		if(m_today != my_tm.tm_mday)
		{
			snprintf(new_log,sizeof(new_log)-1,"%s%s%s",dir_name,tail,log_name);
			m_today = my_tm.tm_mday;
			m_count = 0;
		}
		else
		{
			snprintf(new_log,sizeof(new_log)-1,"%s%s%s.%lld",dir_name,tail,log_name,m_count/m_split_lines);
		}
		m_fp = fopen(new_log,"a");
	}
	m_mutex.unlock();

	va_list arg_list;
	va_start(arg_list,format);

	string log_str;

	m_mutex.lock();
	int n = snprintf(m_buf,48,"%d-%02d-%02d %02d:%02d:%02d.%06ld %s ",my_tm.tm_year + 1900,my_tm.tm_mon+1,
			my_tm.tm_mday,my_tm.tm_hour,my_tm.tm_min,my_tm.tm_sec,now.tv_usec,s);
	int m = vsnprintf(m_buf+n,m_log_buf_size-n-1,format,arg_list);
	m_buf[n+m] = '\n';
	m_buf[n+m+1] = '\0';
	log_str = m_buf;
	m_mutex.unlock();

	va_end(arg_list);

	if(m_is_async && !m_log_queue->full())
	{
		m_log_queue->push(log_str);

	}
	else
	{
		m_mutex.lock();
		fputs(log_str.c_str(),m_fp);
		m_mutex.unlock();
	}
}

void Log::flush(void)
{
	m_mutex.lock();
	fflush(m_fp);
	m_mutex.unlock();
}


















