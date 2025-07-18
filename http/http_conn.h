#ifndef HTTP_CONN_H
#define HTTP_CONN_H

#include<stdio.h>
#include<stdlib.h>
#include<arpa/inet.h>
#include<sys/socket.h>
#include<sys/epoll.h>
#include<sys/stat.h>
#include<sys/mman.h>
#include<sys/wait.h>
#include<sys/uio.h>
#include<fcntl.h>
#include<signal.h>
#include<unistd.h>
#include<string.h>
#include<stdarg.h>
#include<errno.h>
#include<pthread.h>
#include<map>

#include "../lock/locker.h"
#include "../CGImysql/sql_connection_pool.h"

using std::map;
using std::string;

class http_conn
{
public:
	static const int FILENAME_LEN = 200;
	static const int READ_BUFFER_SIZE = 2048;
	static const int WRITE_BUFFER_SIZE = 1024;
	enum METHOD
	{
		GET = 0,POST,HEAD,PUT,DELETE,TRACE,OPTIONS,CONNECT,PATH
	};
	enum CHECK_STATE
	{
		CHECK_STATE_REQUESTLINE = 0,CHECK_STATE_HEADER,CHECK_STATE_CONTENT
	};
	enum HTTP_CODE
	{
		NO_REQUEST,GET_REQUEST,BAD_REQUEST,NO_RESOURCE,FORBIDDEN_REQUEST,FILE_REQUEST,INTERNAL_ERROR,CLOSED_CONNECTION
	};
	enum LINE_STATUS
	{
		LINE_OK = 0,LINE_BAD,LINE_OPEN
	};

public:
	http_conn(){}
	~http_conn(){}

public:
	void init(int sockfd,const struct sockaddr_in& addr);
	void close_conn(bool real_close = true);
	void process();
	bool read_once();
	bool write();
	struct sockaddr_in* get_address()
	{
		return &m_address;
	}
	void initmysql_result(connection_pool* connpool);

private:
	void init();
	HTTP_CODE process_read();
	bool process_write(HTTP_CODE ret);

	HTTP_CODE parse_request_line(char* text);
	HTTP_CODE parse_headers(char* text);
	HTTP_CODE parse_content(char* text);
	HTTP_CODE do_request();
	char* get_line() { return m_read_buf+m_start_line; }
	LINE_STATUS parse_line();

	void unmap();
	bool add_response(const char* format,...);
	bool add_content(const char* content);
	bool add_status_line(int status,const char* title);
	bool add_headers(int content_length);
	bool add_content_type();
	bool add_content_length(int content_length);
	bool add_linger();
	bool add_blank_line();

public:
	static int m_epollfd;
	static int m_user_count;
	static map<string,string> m_users;
	static locker m_mutex;
	MYSQL* m_mysql;

private:
	int m_sockfd;
	struct sockaddr_in m_address;
	
	char m_read_buf[READ_BUFFER_SIZE];
	int m_read_idx;
	int m_checked_idx;
	int m_start_line;
	char m_write_buf[WRITE_BUFFER_SIZE];
	int m_write_idx;
	CHECK_STATE m_check_state;
	
	METHOD m_method;
	char m_real_file[FILENAME_LEN];

	char* m_url;
	char* m_version;
	char* m_host;
	int m_content_length;
	bool m_linger;
	char* m_file_address;
	struct stat m_file_stat;
	struct iovec m_iv[2];
	int m_iv_count;

	int m_cgi;
	char* m_string;
	int m_bytes_to_send;
	int m_bytes_have_send;

};
#endif
