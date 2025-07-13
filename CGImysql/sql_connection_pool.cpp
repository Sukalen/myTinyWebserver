#include<mysql/mysql.h>
#include<stdio.h>
#include<string.h>
#include<string>
#include<list>
#include<stdlib.h>
#include<iostream>

#include "sql_connection_pool.h"

using std::string;
using std::list;


connection_pool::connection_pool()
{
	m_curconn = 0;
	m_freeconn = 0;
}

connection_pool::~connection_pool()
{
	destroy_pool();
}

connection_pool* connection_pool::get_instance()
{
	static connection_pool conn_pool;
	return &conn_pool;
}

void connection_pool::init(string url,string user,string password,string dbname,int port,unsigned int maxconn)
{
	m_url = url;
	m_port = port;
	m_user = user;
	m_password = password;
	m_dbname = dbname;

	m_mutex.lock();
	for(int i=0;i<maxconn;++i)
	{
		MYSQL* con = NULL;
		con = mysql_init(con);

		if(NULL == con)
		{
			LOG_ERROR("Mysql error:%s",mysql_error(con));
			Log::get_instance()->flush();
			exit(1);
		}
		con = mysql_real_connect(con,m_url.c_str(),m_user.c_str(),m_password.c_str(),m_dbname.c_str(),m_port,NULL,0);
		if(NULL == con)
		{
			LOG_ERROR("Mysql error:%s",mysql_error(con));
			Log::get_instance()->flush();
			exit(1);
		}
		m_connlist.push_back(con);
		++m_freeconn;
	}
	m_reserve = sem(m_freeconn);
	m_maxconn = m_freeconn;
	m_mutex.unlock();
}

MYSQL* connection_pool::get_connection()
{
	MYSQL* con = NULL;
	if(0 == m_connlist.size())
	{
		return NULL;
	}

	m_reserve.wait();
	
	m_mutex.lock();
	con = m_connlist.front();
	m_connlist.pop_front();
	--m_freeconn;
	++m_curconn;
	m_mutex.unlock();

	return con;
}

bool connection_pool::release_connection(MYSQL* con)
{
	if(NULL == con)
	{
		return false;
	}

	m_mutex.lock();
	m_connlist.push_back(con);
	++m_freeconn;
	--m_curconn;
	m_mutex.unlock();

	m_reserve.post();
	return true;
}

void connection_pool::destroy_pool()
{
	m_mutex.lock();
	if(m_connlist.size() > 0)
	{
		list<MYSQL*>::iterator it;
		for(it=m_connlist.begin();it!=m_connlist.end();++it)
		{
			MYSQL* con = *it;
			mysql_close(con);
		}
		m_curconn = 0;
		m_freeconn = 0;
		m_maxconn = 0;
		m_connlist.clear();
	}
	m_mutex.unlock();
}

int connection_pool::get_free_conn()
{
	return m_freeconn;
}

connectionRAII::connectionRAII(MYSQL** con,connection_pool* conn_pool)
{
	*con = conn_pool->get_connection();

	m_conRAII = *con;
	m_poolRAII = conn_pool;
}

connectionRAII::~connectionRAII()
{
	m_poolRAII->release_connection(m_conRAII);
}





