#ifndef SQL_CONNECTION_POOL_H
#define SQL_CONNECTION_POOL_H

#include<stdio.h>
#include<list>
#include<mysql/mysql.h>
#include<error.h>
#include<string.h>
#include<iostream>
#include<string>

#include "../lock/locker.h"
#include "../log/log.h"

using std::string;
using std::list;

class connection_pool
{
public:
	MYSQL* get_connection();
	bool release_connection(MYSQL* conn);
	int get_free_conn();
	void destroy_pool();

	static connection_pool* get_instance();
	void init(string url,string user,string password,string dbname,int port,unsigned int maxconn);

	connection_pool();
	~connection_pool();

private:
	unsigned int m_maxconn;
	unsigned int m_curconn;
	unsigned int m_freeconn;
private:
	locker m_mutex;
	list<MYSQL*> m_connlist;
	sem m_reserve;

private:
	string m_url;
	string m_port;
	string m_user;
	string m_password;
	string m_dbname;
};

class connectionRAII
{
public:
	connectionRAII(MYSQL** con,connection_pool* conn_pool);
	~connectionRAII();
private:
	MYSQL* m_conRAII;
	connection_pool* m_poolRAII;
};




#endif

