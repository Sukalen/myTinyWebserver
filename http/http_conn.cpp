#include<mysql/mysql.h>
#include<fstream>

#include "http_conn.h"
#include "../log/log.h"


#define connfdET

//define connfdLT


#define listenfdET
//#define listenfdLT

using std::map;
using std::string;
using std::pair;

const char* ok_200_title = "OK";
const char* error_400_title = "Bad Request";
const char* error_400_form = "Your request has bad syntax or is inherently impossible to satisfy.\n";
const char* error_403_title = "Forbidden";
const char* error_403_form = "You do not have permission to get file from this server.\n";
const char* error_404_title = "Not Found";
const char* error_404_form = "The requested file was not found on this server.\n";
const char* error_500_title = "Internal Error";
const char* error_500_form = "There was an unusual problem serving the requested file.\n";

const char* doc_root = "/home/suu/myworkspace/myTinyWebServer/root";

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
int http_conn::m_user_count = 0;
map<string,string> http_conn::m_users;
locker http_conn::m_mutex;

void http_conn::init()
{
	memset(m_read_buf,'\0',READ_BUFFER_SIZE);
	memset(m_write_buf,'\0',WRITE_BUFFER_SIZE);
	memset(m_real_file,'\0',FILENAME_LEN);
	
	m_mysql = NULL;
	m_read_idx = 0;
	m_checked_idx = 0;
	m_start_line = 0;
	m_write_idx = 0;
	m_check_state = CHECK_STATE_REQUESTLINE;
	m_method = GET;
	m_url = NULL;
	m_version = NULL;
	m_host = NULL;
	m_content_length = 0;
	m_linger = false;
	m_cgi = 0;
	m_bytes_to_send = 0;
	m_bytes_have_send = 0;
}

void http_conn::init(int sockfd,const struct sockaddr_in& addr)
{
	m_sockfd = sockfd;
	m_address = addr;
#ifdef connfdET
	addfd(m_epollfd,sockfd,true,true);
#endif

#ifdef connfdLT
	addfd(m_epollfd,sockfd,false,true);
#endif

	m_user_count++;
	init();
}
	
void http_conn::close_conn(bool real_close)
{
	if(real_close && (m_sockfd!=-1))
	{
		removefd(m_epollfd,m_sockfd);
		m_sockfd = -1;
		m_user_count--;
	}
}

void http_conn::initmysql_result(connection_pool* connpool)
{
	MYSQL* mysql = NULL;
	connectionRAII mysqlcon(&mysql,connpool);

	if(mysql_query(mysql,"SELECT username,passwd FROM user"))
	{
		LOG_ERROR("SELECT error:%s\n",mysql_error(mysql));
	}

	MYSQL_RES* result = mysql_store_result(mysql);
	//int num_fields = mysql_num_fields(result);
	//MYSQL_FIELD* fields = mysql_fetch_fields(result);

	while(MYSQL_ROW row = mysql_fetch_row(result))
	{
		string temp1(row[0]);
		string temp2(row[1]);
		m_users[temp1] = temp2;
	}
}

http_conn::LINE_STATUS http_conn::parse_line()
{
	char temp;
	for(;m_checked_idx<m_read_idx;++m_checked_idx)
	{
		temp = m_read_buf[m_checked_idx];
		if('\r' == temp)
		{
			if( m_checked_idx + 1 == m_read_idx)
			{
				return LINE_OPEN;
			}
			else if('\n' == m_read_buf[m_checked_idx+1])
			{
				m_read_buf[m_checked_idx++] = '\0';
				m_read_buf[m_checked_idx++] = '\0';
				return LINE_OK;
			}
			return LINE_BAD;
		}
		else if('\n' == temp)
		{
			if(m_checked_idx > 1 && '\r' == m_read_buf[m_checked_idx-1])
			{
				m_read_buf[m_checked_idx-1] = '\0';
				m_read_buf[m_checked_idx++] = '\0';
				return LINE_OK;
			}
			return LINE_BAD;
		}
	}
	return LINE_OPEN;
}

http_conn::HTTP_CODE http_conn::parse_request_line(char* text)
{
	m_url = strpbrk(text," \t");
	if(!m_url)
	{
		return BAD_REQUEST;
	}

	*m_url++ = '\0';
	char* method = text;
	if(0 == strcasecmp(method,"GET"))
	{
		m_method = GET;
	}
	else if(0 == strcasecmp(method,"POST"))
	{
		m_method = POST;
		m_cgi = 1;
	}
	else
	{
		return BAD_REQUEST;
	}

	m_url += strspn(m_url," \t");
	m_version = strpbrk(m_url," \t");
	if(!m_version)
	{
		return BAD_REQUEST;
	}
	*m_version++ = '\0';
	m_version+=strspn(m_version," \t");
	if(strcasecmp(m_version,"HTTP/1.1")!=0)
	{
		return BAD_REQUEST;
	}

	if(0 == strncasecmp(m_url,"http://",7))
	{
		m_url+=7;
		m_url = strchr(m_url,'/');
	}
	else if(0 == strncasecmp(m_url,"https://",8))
	{
		m_url+=8;
		m_url = strchr(m_url,'/');
	}

	if(!m_url || m_url[0]!='/')
	{
		return BAD_REQUEST;
	}

	if(1 == strlen(m_url))
	{
		strcat(m_url,"judge.html");
	}
	m_check_state = CHECK_STATE_HEADER;
	return NO_REQUEST;
}

http_conn::HTTP_CODE http_conn::parse_headers(char* text)
{
	if('\0' == text[0])
	{
		if(m_content_length != 0)
		{
			m_check_state = CHECK_STATE_CONTENT;
			return NO_REQUEST;
		}
		return GET_REQUEST;
	}
	else if( 0 == strncasecmp(text,"Connection:",11))
	{
		text+=11;
		text+=strspn(text," \t");
		if(0 == strcasecmp(text,"keep-alive"))
		{
			m_linger = true;
		}
	}
	else if( 0 == strncasecmp(text,"Content-length:",15))
	{
		text+=15;
		text+=strspn(text," \t");
		m_content_length = atoi(text);
	}
	else if( 0 == strncasecmp(text,"Host:",5))
	{
		text+=5;
		text+=strspn(text," \t");
		m_host = text;
	}
	else
	{
		LOG_INFO("oop!unknow header:%s",text);
		Log::get_instance()->flush();
	}
	return NO_REQUEST;
}

http_conn::HTTP_CODE http_conn::parse_content(char* text)
{
	if(m_read_idx >= m_content_length+m_checked_idx)
	{
		text[m_content_length] = '\0';
		m_string = text;
		return GET_REQUEST;
	}
	return NO_REQUEST;
}

http_conn::HTTP_CODE http_conn::do_request()
{
	strcpy(m_real_file,doc_root);
	int len = strlen(doc_root);
	const char* p = strrchr(m_url,'/');

	if(1 == m_cgi && ('2' == *(p+1) || '3' == *(p+1)))
	{
		char* real_url = (char*)malloc(sizeof(char)*200);
		strcpy(real_url,"/");
		strcat(real_url,m_url+2);
		strncpy(m_real_file+len,real_url,FILENAME_LEN-len-1);
		free(real_url);

		char name[100],password[100];
		int i;
		for(i=5;m_string[i]!='&';++i)
			name[i-5] = m_string[i];
		name[i-5]='\0';
		int j=0;
		for(i=i+10;m_string[i]!='\0';++i,++j)
			password[j] = m_string[i];
		password[j] = '\0';

		if('2' == *(p+1))
		{
			if(m_users.find(name) != m_users.end() && m_users[name]==password)
			{
				strcpy(m_url,"/welcome.html");
			}
			else
			{
				strcpy(m_url,"/logError.html");
			}
		}
		else if('3' == *(p+1))
		{
			if(m_users.find(name) == m_users.end())
			{
				char* sql_insert = (char*)malloc(sizeof(char)*200);
				strcpy(sql_insert, "INSERT INTO user(username, passwd) VALUES(");
            	strcat(sql_insert, "'");
            	strcat(sql_insert, name);
            	strcat(sql_insert, "', '");
            	strcat(sql_insert, password);
            	strcat(sql_insert, "')");

				m_mutex.lock();
				int res = mysql_query(m_mysql,sql_insert);
				m_users.insert(pair<string,string>(name,password));
				m_mutex.unlock();
				
				free(sql_insert);

				if(!res)
				{
					strcpy(m_url,"/log.html");
				}
				else
				{
					strcpy(m_url,"/registerError.html");
				}
			}
			else
			{
				strcpy(m_url,"/registerError.html");
			}
		}
	}
	if('0' == *(p+1))
	{
		char* real_url = (char*)malloc(sizeof(char)*200);
		strcpy(real_url,"/register.html");
		strncpy(m_real_file+len,real_url,strlen(real_url));
		free(real_url);

	}
	else if('1' == *(p+1))
	{
		char* real_url = (char*)malloc(sizeof(char)*200);
		strcpy(real_url,"/log.html");
		strncpy(m_real_file+len,real_url,strlen(real_url));
		free(real_url);

	}
	else if('5' == *(p+1))
	{
		char* real_url = (char*)malloc(sizeof(char)*200);
		strcpy(real_url,"/picture.html");
		strncpy(m_real_file+len,real_url,strlen(real_url));
		free(real_url);

	}
	else if('6' == *(p+1))
	{
		char* real_url = (char*)malloc(sizeof(char)*200);
		strcpy(real_url,"/video.html");
		strncpy(m_real_file+len,real_url,strlen(real_url));
		free(real_url);

	}
	else
	{
		strncpy(m_real_file+len,m_url,FILENAME_LEN-len-1);
	}

	if(stat(m_real_file,&m_file_stat) < 0)
	{
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

	int fd = open(m_real_file,O_RDONLY);
	m_file_address = (char*)mmap(0,m_file_stat.st_size,PROT_READ,MAP_PRIVATE,fd,0);
	close(fd);
	return FILE_REQUEST;
}

http_conn::HTTP_CODE http_conn::process_read()
{
	LINE_STATUS line_status = LINE_OK;
	HTTP_CODE ret = NO_REQUEST;
	char* text = 0;

	while((CHECK_STATE_CONTENT == m_check_state && LINE_OK == line_status) || (LINE_OK == (line_status=parse_line())))
	{
		text = get_line();
		m_start_line = m_checked_idx;
		LOG_INFO("%s",text);
		Log::get_instance()->flush();
		
		switch(m_check_state)
		{
			case CHECK_STATE_REQUESTLINE:
			{
            	ret = parse_request_line(text);
            	if (ret == BAD_REQUEST)
                	return BAD_REQUEST;
            	break;
        	}
        	case CHECK_STATE_HEADER:
	        {
    	        ret = parse_headers(text);
        	    if (ret == BAD_REQUEST)
	                return BAD_REQUEST;
    	        else if (ret == GET_REQUEST)
    	        {
   		             return do_request();
        	    }
        	    break;
      	  	}
 	       	case CHECK_STATE_CONTENT:
   		   	{
        	    ret = parse_content(text);
          	    if (ret == GET_REQUEST)
            	    return do_request();
            	line_status = LINE_OPEN;
            	break;
        	}
     	   	default:
            	return INTERNAL_ERROR;
        }
	}
	return NO_REQUEST;
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

			if(m_linger)
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
    add_content_length(content_len);
    add_linger();
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
    return add_response("Connection:%s\r\n", (m_linger == true) ? "keep-alive" : "close");
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
	bool write_ret = process_write(read_ret);
	if(!write_ret)
	{
		close_conn();
	}
	modfd(m_epollfd,m_sockfd,EPOLLOUT);
}


