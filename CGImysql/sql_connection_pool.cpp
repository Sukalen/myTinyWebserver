#include<mysql/mysql.h>

#include<condition_variable>
#include<cstddef>
#include<memory>
#include<mutex>
#include<queue>
#include<vector>
#include<iostream>
#include<string>

#include "../log/log.h"

#include "sql_connection_pool.h"


connection_pool::~connection_pool()
{
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_stopping = true;
    }
    m_cond.notify_all();
}

connection_pool* connection_pool::get_instance()
{
	static connection_pool conn_pool;
	return &conn_pool;
}

void connection_pool::init(std::string url, std::string user,
        std::string password, std::string dbname, int port, unsigned int maxconn)
{
    if(0 == maxconn)
    {
        throw std::invalid_argument("maxconn must be positive");
    }
    std::vector<MysqlPtr> connections;
    std::queue<MYSQL*> free_connections;

    connections.reserve(maxconn);
    
    for(unsigned int i=0; i < maxconn; ++i)
    {
        MysqlPtr conn(mysql_init(nullptr));

        if(!conn)
        {
            throw std::runtime_error("mysql_init failed");
        }

        if(!mysql_real_connect(
                    conn.get(),
                    url.c_str(),
                    user.c_str(),
                    password.c_str(),
                    dbname.c_str(),
                    port,
                    nullptr,
                    0))
        {
            std::string error = mysql_error(conn.get());

            throw std::runtime_error("mysql_real_connect failed:"+error);
        }

        free_connections.push(conn.get());

        connections.push_back(std::move(conn));
    }

    {
        std::lock_guard<std::mutex> lock(m_mutex);

        if(m_initialized)
        {
            throw std::logic_error("connection_pool already initialized");
        }


	    m_url = std::move(url);
	    m_user = std::move(user);
	    m_password = std::move(password);
	    m_dbname = std::move(dbname);
	    m_port = port;
        
        m_connections = std::move(connections);
        m_free_connections = std::move(free_connections);

        m_stopping = false;
        m_initialized = true;
    }
}

MYSQL* connection_pool::get_connection()
{
    std::unique_lock<std::mutex> lock(m_mutex);
    m_cond.wait(lock,[this]{ return m_stopping || !m_free_connections.empty();});

    if(m_stopping)
    {
        return nullptr;
    }

    MYSQL* conn = m_free_connections.front();
    m_free_connections.pop();

    return conn;

}

bool connection_pool::release_connection(MYSQL* conn)
{
	if(nullptr == conn)
	{
		return false;
	}
    
    {
        std::lock_guard<std::mutex> lock(m_mutex);

        if(m_stopping)
        {
            return false;
        }

        m_free_connections.push(conn);
    }

    m_cond.notify_one();

    return true;
}


int connection_pool::get_free_conn() const
{
    std::lock_guard<std::mutex> lock(m_mutex);

	return static_cast<int>(m_free_connections.size());
}

connectionRAII::connectionRAII(MYSQL** con,connection_pool* conn_pool)
{
    if(!con || !conn_pool)
    {
        throw std::invalid_argument(
            "invalid connectionRAII argument");
    }

    m_conRAII =
        conn_pool->get_connection();

    if(!m_conRAII)
    {
        throw std::runtime_error(
            "failed to acquire mysql connection");
    }

    m_poolRAII = conn_pool;

    *con = m_conRAII;
}

connectionRAII::~connectionRAII()
{
    if(m_poolRAII && m_conRAII)
	{
        m_poolRAII->release_connection(m_conRAII);
    }
}





