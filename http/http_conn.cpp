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



int http_conn::m_epollfd = -1;
std::atomic<int> http_conn::m_user_count{0};

void http_conn::init()
{
	memset(m_read_buf,'\0',READ_BUFFER_SIZE);
	
	m_read_idx = 0;

	m_request.reset();
	m_response.reset();
	
	m_file.reset();

	m_iv_count = 0;

	m_bytes_to_send = 0;
}

void http_conn::init(
		int sockfd,
	   	const struct sockaddr_in& addr, 
		AuthService* auth_service,
	   	StaticFileHandler* static_file_handler)
{
	m_sockfd = sockfd;
	m_address = addr;

	m_auth_service = auth_service;

	m_static_file_handler = static_file_handler;

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
		m_file.reset();

		removefd(m_epollfd, m_sockfd);

		m_sockfd = -1;

		m_user_count.fetch_sub(1, std::memory_order_relaxed);
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
	Router::RouteResult route = m_router.resolve(m_request);
	std::string target_url = route.target;

    if( Router::RouteType::Login == route.type ||
		   	Router::RouteType::Register == route.type)
    {
        std::string username;
        std::string password;

        if(!parse_user_form(username, password))
        {
            return BAD_REQUEST;
        }

        
        if(Router::RouteType::Login == route.type)
        {

			if(!m_auth_service)
			{
				return INTERNAL_ERROR;
			}

            bool login_success = m_auth_service->login(
					username,
					password);

            target_url =
                login_success ? "/welcome.html" : "/logError.html";
        }
        else
        {
			if(!m_auth_service)
			{
				return INTERNAL_ERROR;
			}

			AuthService::RegisterResult result =
			   	m_auth_service->register_user(username, password);

			switch(result)
			{
				case AuthService::RegisterResult::Success:
				{
					target_url = "/log.html";
					break;
				}

				case AuthService::RegisterResult::AlreadyExists:
        		{
            		target_url = "/registerError.html";
            		break;
       	 		}

        		case AuthService::RegisterResult::DatabaseError:
        		{
            		return INTERNAL_ERROR;
        		}
        	}
		}
    }

	if(!m_static_file_handler)
	{
		return INTERNAL_ERROR;
	}

	StaticFileHandler::LoadResult
		file_result = m_static_file_handler->load(target_url);

	switch(file_result.result)
	{
		case StaticFileHandler::Result::Success:
		{
			m_file = std::move(file_result.file);
			return FILE_REQUEST;
		}
	    case StaticFileHandler::Result::NotFound:
    	{
        	return NO_RESOURCE;
    	}

    	case StaticFileHandler::Result::Forbidden:
    	{
        	return FORBIDDEN_REQUEST;
    	}

    	case StaticFileHandler::Result::BadRequest:
    	{
        	return BAD_REQUEST;
    	}

    	case StaticFileHandler::Result::InternalError:
    	{
        	return INTERNAL_ERROR;
    	}
	}

	return INTERNAL_ERROR;
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
	while(m_bytes_to_send > 0)
	{
		ssize_t bytes_sent = writev(
				m_sockfd, m_iv, m_iv_count);

		if(bytes_sent < 0)
		{
			if(EAGAIN == errno || EWOULDBLOCK == errno)
			{
				modfd(m_epollfd, m_sockfd, EPOLLOUT);
				return true;
			}

			m_file.reset();
			return false;
		}

		if( 0 == bytes_sent)
		{
			m_file.reset();
			return false;
		}

		advance_iovecs(static_cast<std::size_t>(bytes_sent));
		m_bytes_to_send -= static_cast<std::size_t>(bytes_sent);
	}

	m_file.reset();

	modfd(m_epollfd, m_sockfd, EPOLLIN);

	if(m_request.keep_alive())
	{
		init();
		return true;
	}

	return false;
}


bool http_conn::process_write(HTTP_CODE ret)
{
    m_response.reset();

    m_response.set_keep_alive(m_request.keep_alive());

    switch(ret)
    {
        case INTERNAL_ERROR:
        {
            m_response.set_status(500, error_500_title);

            m_response.set_content_type("text/plain; charset=utf-8");

            m_response.set_body(error_500_form);
            break;
        }

        case BAD_REQUEST:
        {
            m_response.set_status(400,error_400_title);

            m_response.set_content_type("text/plain; charset=utf-8");

            m_response.set_body(error_400_form);

            break;
        }

        case NO_RESOURCE:
        {
            m_response.set_status(404, error_404_title);

            m_response.set_content_type("text/plain; charset=utf-8");

            m_response.set_body(error_404_form);

            break;
        }

        case FORBIDDEN_REQUEST:
        {
            m_response.set_status(403, error_403_title);

            m_response.set_content_type("text/plain; charset=utf-8");

            m_response.set_body(error_403_form);
            break;
        }

        case FILE_REQUEST:
        {
            m_response.set_status(200,ok_200_title);

            if(!m_file.empty())
            {
                m_response.set_content_length(m_file.size());
            }
            else
            {
                m_response.set_content_type("text/html; charset=utf-8");

                m_response.set_body("<html><body></body></html>");
            }

            break;
        }

        default:
            return false;
    }

    m_response.build();

    m_iv[0].iov_base = const_cast<char*>(m_response.header().data());

    m_iv[0].iov_len = m_response.header().size();

    m_iv_count = 1;

    m_bytes_to_send = m_iv[0].iov_len;

    if(FILE_REQUEST == ret &&
       !m_file.empty())
    {
        m_iv[1].iov_base = m_file.data();

        m_iv[1].iov_len = m_file.size();

        m_iv_count = 2;

        m_bytes_to_send += m_iv[1].iov_len;
    }

    else if(!m_response.body().empty())
    {
        m_iv[1].iov_base =const_cast<char*>(m_response.body().data());

        m_iv[1].iov_len = m_response.body().size();

        m_iv_count = 2;

        m_bytes_to_send += m_iv[1].iov_len;
    }

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

void http_conn::advance_iovecs(std::size_t bytes)
{
	for(int i = 0; i < m_iv_count && bytes > 0; ++i)
	{
		if( 0 == m_iv[i].iov_len)
		{
			continue;
		}

		if(bytes >= m_iv[i].iov_len)
		{
			bytes -= m_iv[i].iov_len;

			m_iv[i].iov_base = 
				static_cast<char*>(m_iv[i].iov_base) + m_iv[i].iov_len;

			m_iv[i].iov_len = 0;
		}
		else
		{
			m_iv[i].iov_base =
				static_cast<char*>(m_iv[i].iov_base) + bytes;

			m_iv[i].iov_len -= bytes;
			bytes = 0;
		}
	}
}


