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
#include<atomic>
#include<mutex>
#include<string>
#include<map>

#include "../CGImysql/sql_connection_pool.h"
#include "http_request.h"
#include "http_response.h"
#include "router.h"
#include "../service/auth_service.h"


class http_conn
{
public:
	static const int FILENAME_LEN = 200;
	static const int READ_BUFFER_SIZE = 2048;
	enum HTTP_CODE
	{
		NO_REQUEST,GET_REQUEST,BAD_REQUEST,NO_RESOURCE,FORBIDDEN_REQUEST,FILE_REQUEST,INTERNAL_ERROR,CLOSED_CONNECTION
	};

public:
	http_conn(){}
	~http_conn(){}

public:
	void init(int sockfd, const struct sockaddr_in& addr, AuthService* auth_service);
	void close_conn(bool real_close = true);
	void process();
	bool read_once();
	bool write();
	struct sockaddr_in* get_address()
	{
		return &m_address;
	}

private:
	void init();
	HTTP_CODE process_read();
	bool process_write(HTTP_CODE ret);

	HTTP_CODE do_request();

	void unmap();

	bool parse_user_form(std::string& username, std::string& password) const;

	void advance_iovecs(std::size_t bytes);

public:
	static int m_epollfd;
	static std::atomic<int> m_user_count;

private:
	int m_sockfd;
	struct sockaddr_in m_address;
	
	char m_read_buf[READ_BUFFER_SIZE];
	int m_read_idx;
	
	char m_real_file[FILENAME_LEN];

	char* m_file_address;
	struct stat m_file_stat;
	struct iovec m_iv[2];
	int m_iv_count;

	std::size_t m_bytes_to_send = 0;
	
	HttpRequest m_request;
	HttpResponse m_response;
	Router m_router;
	AuthService* m_auth_service = nullptr;

};
#endif
