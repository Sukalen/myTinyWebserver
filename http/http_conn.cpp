#include<mysql/mysql.h>
#include<fstream>
#include<string>
#include<map>
#include<mutex>
#include<atomic>
#include<memory>


#include "http_conn.h"
#include "../log/log.h"


#define connfdET

//define connfdLT


#define listenfdET
//#define listenfdLT


const char* ok_200_title = "OK";
const char* error_400_title = "Bad Request";
const char* error_400_form = "Your request has bad syntax or is inherently impossible to satisfy.\n";
const char* error_403_title = "Forbidden";
const char* error_403_form = "You do not have permission to get file from this server.\n";
const char* error_404_title = "Not Found";
const char* error_404_form = "The requested file was not found on this server.\n";
const char* error_500_title = "Internal Error";
const char* error_500_form = "There was an unusual problem serving the requested file.\n";

const char* doc_root = "/home/suu/myworkspace/myTinyWebserver/root";

int setnonblocking(int fd)
{
	int old_option = fcntl(fd,F_GETFL);
	int new_option = old_option | O_NONBLOCK;
	fcntl(fd,F_SETFL,new_option);
	return old_option;
}

void addfd(int epollfd, int fd, bool is_et, bool one_shot)
{
	struct epoll_event event;
	event.data.fd = fd;

	event.events = EPOLLIN|EPOLLRDHUP;
	if(is_et)
	{
		event.events|=EPOLLET;
	}
	if(one_shot)
	{
		event.events|=EPOLLONESHOT;
	}
	epoll_ctl(epollfd,EPOLL_CTL_ADD,fd,&event);
	setnonblocking(fd);
}

void removefd(int epollfd,int fd)
{
	epoll_ctl(epollfd,EPOLL_CTL_DEL,fd,0);
	close(fd);
}

void modfd(int epollfd,int fd,int ev)
{
	struct epoll_event event;
	event.data.fd = fd;

#ifdef connfdET
	event.events = ev|EPOLLONESHOT|EPOLLRDHUP|EPOLLET;
#endif

#ifdef connfdLT
	event.events = ev|EPOLLONESHOT|EPOLLRDHUP;
#endif
	
	epoll_ctl(epollfd,EPOLL_CTL_MOD,fd,&event);
}



namespace
{
struct MysqlResultDeleter
{
	void operator()(MYSQL_RES* result) const noexcept
	{
		if(result)
		{
			mysql_free_result(result);
		}
	}
};

using MysqlResultPtr = std::unique_ptr<MYSQL_RES, MysqlResultDeleter>;
}




int http_conn::m_epollfd = -1;
std::atomic<int> http_conn::m_user_count{0};
std::map<std::string, std::string> http_conn::m_users;
std::mutex http_conn::m_mutex;

void http_conn::init()
{
	memset(m_read_buf,'\0',READ_BUFFER_SIZE);
	memset(m_write_buf,'\0',WRITE_BUFFER_SIZE);
	memset(m_real_file,'\0',FILENAME_LEN);
	
	m_mysql = nullptr;

	m_read_idx = 0;

	m_write_idx = 0;

	m_request.reset();

	m_file_address = nullptr;

	m_iv_count = 0;

	m_bytes_to_send = 0;
	m_bytes_have_send = 0;
}

void http_conn::init(int sockfd, const struct sockaddr_in& addr)
{
	m_sockfd = sockfd;
	m_address = addr;
#ifdef connfdET
	addfd(m_epollfd,sockfd,true,true);
#endif

#ifdef connfdLT
	addfd(m_epollfd,sockfd,false,true);
#endif

	m_user_count.fetch_add(1, std::memory_order_relaxed);

	init();
}
	
void http_conn::close_conn(bool real_close)
{
	if(real_close && m_sockfd!=-1)
	{
		removefd(m_epollfd,m_sockfd);

		m_sockfd = -1;

		m_user_count.fetch_sub(1, std::memory_order_relaxed);
	}
}

void http_conn::initmysql_result(connection_pool* connpool)
{
	MYSQL* mysql = nullptr;

	connectionRAII mysqlcon(&mysql, connpool);

	if(mysql_query(mysql,"SELECT username,passwd FROM user") != 0)
	{
		LOG_ERROR("SELECT error: %s", mysql_error(mysql));
		return;
	}

	MysqlResultPtr result(mysql_store_result(mysql));

	if(!result)
	{
		LOG_ERROR("mysql_store_result failed: %s", mysql_error(mysql));
		return;
	}

	std::map<std::string, std::string> loaded_users;

	while(MYSQL_ROW row = mysql_fetch_row(result.get()))
	{
		if(!row[0] || !row[1])
		{
			continue;
		}

		loaded_users.emplace(row[0], row[1]);
	}

	{
		std::lock_guard<std::mutex> lock(m_mutex);

		m_users = std::move(loaded_users);
	}

}
bool http_conn::parse_user_form(std::string& username, std::string& password) const
{
	const std::string& body = m_request.body();

	if(body.empty())
	{
		return false;
	}

	std::size_t amp = body.find('&');

	if(std::string::npos == amp)
	{
		return false;
	}

	std::size_t first_equal = body.find('=');

	std::size_t second_equal = body.find('=', amp+1);

	if( std::string::npos == first_equal ||
			std::string::npos == second_equal ||
			first_equal >= amp)
	{
		return false;
	}

	username = body.substr(first_equal + 1, amp-first_equal-1);

	password = body.substr(second_equal+1);

	return !username.empty() && !password.empty();
}




http_conn::HTTP_CODE http_conn::do_request()
{
	const std::string& request_url = m_request.url();
    if(request_url.empty())
    {
        return BAD_REQUEST;
    }

    std::size_t slash = request_url.find_last_of('/');

    if(std::string::npos == slash)
    {
        return BAD_REQUEST;
    }

    char route = '\0';

	if(slash + 1 < request_url.size())
	{
		route = request_url[slash+1];
	}

    std::string target_url = request_url;

    if(HttpRequest::Method::Post == m_request.method() &&
       ('2' == route || '3' == route))
    {
        std::string username;
        std::string password;

        if(!parse_user_form(username, password))
        {
            return BAD_REQUEST;
        }

        
        if('2' == route)
        {
            bool login_success = false;

            {
                std::lock_guard<std::mutex> lock(m_mutex);

                auto it = m_users.find(username);

                login_success =
                    it != m_users.end() && it->second == password;
            }

            target_url =
                login_success ? "/welcome.html" : "/logError.html";
        }

        
        else
        {
            bool already_exists = false;

            {
                std::lock_guard<std::mutex> lock(m_mutex);

                already_exists =
                    m_users.find(username) != m_users.end();
            }

            if(already_exists)
            {
                target_url = "/registerError.html";
            }
            else
            {
                std::string sql_insert =
                    "INSERT INTO user(username, passwd) "
                    "VALUES('" +
                    username +
                    "', '" +
                    password +
                    "')";

                int res = mysql_query(
                        m_mysql,
                        sql_insert.c_str());

                if(0 == res)
                {
                    {
                        std::lock_guard<std::mutex> lock(m_mutex);

                        m_users.emplace(
                            username,
                            password);
                    }

                    target_url = "/log.html";
                }
                else
                {
                    LOG_ERROR(
                        "insert user failed:%s",
                        mysql_error(m_mysql));

                    target_url = "/registerError.html";
                }
            }
        }
    }

    if('0' == route)
    {
        target_url = "/register.html";
    }
    else if('1' == route)
    {
        target_url = "/log.html";
    }
    else if('5' == route)
    {
        target_url = "/picture.html";
    }
    else if('6' == route)
    {
        target_url = "/video.html";
    }

    std::string full_path =
        std::string(doc_root) + target_url;

    if(full_path.size() >= FILENAME_LEN)
    {
        return BAD_REQUEST;
    }

    std::snprintf(
        m_real_file,
        FILENAME_LEN,
        "%s",
        full_path.c_str());

    if(stat(
            m_real_file,
            &m_file_stat) < 0)
    {
        LOG_ERROR(
            "stat failed: file=%s errno=%d error=%s",
            m_real_file,
            errno,
            strerror(errno));

        return NO_RESOURCE;
    }

    if(!(m_file_stat.st_mode & S_IROTH))
    {
        return FORBIDDEN_REQUEST;
    }

    if(S_ISDIR(m_file_stat.st_mode))
    {
        return BAD_REQUEST;
    }

    if(0 == m_file_stat.st_size)
    {
        m_file_address = nullptr;
        return FILE_REQUEST;
    }

    int fd = open(m_real_file, O_RDONLY);

    if(fd < 0)
    {
        if(EACCES == errno)
        {
            return FORBIDDEN_REQUEST;
        }

        return INTERNAL_ERROR;
    }

    void* mapped = mmap(
            nullptr,
            m_file_stat.st_size,
            PROT_READ,
            MAP_PRIVATE,
            fd,
            0);

    close(fd);

    if(MAP_FAILED == mapped)
    {
        m_file_address = nullptr;

        LOG_ERROR(
            "mmap failed: file=%s errno=%d error=%s",
            m_real_file,
            errno,
            strerror(errno));

        return INTERNAL_ERROR;
    }

    m_file_address = static_cast<char*>(mapped);

    return FILE_REQUEST;
}

http_conn::HTTP_CODE http_conn::process_read()
{
	HttpRequest::ParseResult result = m_request.parse(
			m_read_buf, static_cast<std::size_t>(m_read_idx));
    
	switch(result)
    {
        case HttpRequest::ParseResult::Incomplete:
            return NO_REQUEST;

        case HttpRequest::ParseResult::BadRequest:
            return BAD_REQUEST;

        case HttpRequest::ParseResult::Complete:
            return do_request();
    }

    return INTERNAL_ERROR;
}

bool http_conn::read_once()
{
	if(m_read_idx >= READ_BUFFER_SIZE)
	{
		return false;
	}
	int bytes_read = 0;

#ifdef connfdET
	while(1)
	{
		bytes_read = recv(m_sockfd,m_read_buf+m_read_idx,READ_BUFFER_SIZE-m_read_idx,0);
		if(-1 == bytes_read)
		{
			if(EAGAIN == errno || EWOULDBLOCK == errno)
			{
				break;
			}
			return false;
		}
		else if(0==bytes_read)
		{
			return false;
		}
		m_read_idx += bytes_read;
	}
	return true;
#endif

#ifdef connfdLT
	bytes_read = recv(m_sockfd,m_read_buf+m_read_idx,READ_BUFFER_SIZE-m_read_idx,0);
	if(bytes_read <= 0)
	{
		return false;
	}
	m_read_idx += bytes_read;
	return true;
#endif

}

bool http_conn::write()
{
	int temp = 0;
	if(0 == m_bytes_to_send)
	{
		modfd(m_epollfd,m_sockfd,EPOLLIN);
		init();
		return true;
	}

	while(1)
	{
		temp = writev(m_sockfd,m_iv,m_iv_count);
		if(temp < 0)
		{
			if(EAGAIN == errno)
			{
				modfd(m_epollfd,m_sockfd,EPOLLOUT);
				return true;
			}
			unmap();
			return false;
		}

		m_bytes_have_send += temp;
		m_bytes_to_send -= temp;
		if(m_bytes_have_send >= m_iv[0].iov_len)
		{
			m_iv[0].iov_len = 0;
			m_iv[1].iov_base = m_file_address + (m_bytes_have_send-m_write_idx);
			m_iv[1].iov_len = m_bytes_to_send;
		}
		else
		{
			m_iv[0].iov_base = m_write_buf+m_bytes_have_send;
			m_iv[0].iov_len = m_iv[0].iov_len - m_bytes_have_send;
		}

		if(m_bytes_to_send <= 0)
		{
			unmap();
			modfd(m_epollfd,m_sockfd,EPOLLIN);

			if(m_request.keep_alive())
			{
				init();
				return true;
			}
			else
			{
				return false;
			}
		}
	}
}

void http_conn::unmap()
{
	if(m_file_address)
	{
		munmap(m_file_address,m_file_stat.st_size);
		m_file_address = NULL;
	}
}

bool http_conn::add_response(const char *format, ...)
{
    if (m_write_idx >= WRITE_BUFFER_SIZE)
        return false;
    va_list arg_list;
    va_start(arg_list, format);
    int len = vsnprintf(m_write_buf + m_write_idx, WRITE_BUFFER_SIZE - 1 - m_write_idx, format, arg_list);
    if (len >= (WRITE_BUFFER_SIZE - 1 - m_write_idx))
    {
        va_end(arg_list);
        return false;
    }
    m_write_idx += len;
    va_end(arg_list);
    LOG_INFO("request:%s", m_write_buf);
    Log::get_instance()->flush();
    return true;
}
bool http_conn::add_status_line(int status, const char *title)
{
    return add_response("%s %d %s\r\n", "HTTP/1.1", status, title);
}
bool http_conn::add_headers(int content_len)
{
    return add_content_length(content_len) &&
    		add_linger() &&
    		add_blank_line();
}
bool http_conn::add_content_length(int content_len)
{
    return add_response("Content-Length:%d\r\n", content_len);
}
bool http_conn::add_content_type()
{
    return add_response("Content-Type:%s\r\n", "text/html");
}
bool http_conn::add_linger()
{
    return add_response("Connection:%s\r\n", m_request.keep_alive() ? "keep-alive" : "close");
}
bool http_conn::add_blank_line()
{
    return add_response("%s", "\r\n");
}
bool http_conn::add_content(const char *content)
{
    return add_response("%s", content);
}
bool http_conn::process_write(HTTP_CODE ret)
{
    switch (ret)
    {
    case INTERNAL_ERROR:
    {
        add_status_line(500, error_500_title);
        add_headers(strlen(error_500_form));
        if (!add_content(error_500_form))
            return false;
        break;
    }
    case BAD_REQUEST:
    {
	add_status_line(400,error_400_title);
	add_headers(strlen(error_400_form));
	if(!add_content(error_400_form))
		return false;
	break;
    }
    case NO_RESOURCE:
    {
        add_status_line(404, error_404_title);
        add_headers(strlen(error_404_form));
        if (!add_content(error_404_form))
            return false;
        break;
    }
    case FORBIDDEN_REQUEST:
    {
        add_status_line(403, error_403_title);
        add_headers(strlen(error_403_form));
        if (!add_content(error_403_form))
            return false;
        break;
    }
    case FILE_REQUEST:
    {
        add_status_line(200, ok_200_title);
        if (m_file_stat.st_size != 0)
        {
            add_headers(m_file_stat.st_size);
            m_iv[0].iov_base = m_write_buf;
            m_iv[0].iov_len = m_write_idx;
            m_iv[1].iov_base = m_file_address;
            m_iv[1].iov_len = m_file_stat.st_size;
            m_iv_count = 2;
            m_bytes_to_send = m_write_idx + m_file_stat.st_size;
            return true;
        }
        else
        {
            const char *ok_string = "<html><body></body></html>";
            add_headers(strlen(ok_string));
            if (!add_content(ok_string))
                return false;
        }
    }
    default:
        return false;
    }
    m_iv[0].iov_base = m_write_buf;
    m_iv[0].iov_len = m_write_idx;
    m_iv_count = 1;
    m_bytes_to_send = m_write_idx;
    return true;
}

void http_conn::process()
{
	HTTP_CODE read_ret = process_read();
	if(NO_REQUEST == read_ret)
	{
		modfd(m_epollfd,m_sockfd,EPOLLIN);
		return;
	}

	if(!process_write(read_ret))
	{
		close_conn();
		return;
	}

	modfd(m_epollfd,m_sockfd,EPOLLOUT);
}


