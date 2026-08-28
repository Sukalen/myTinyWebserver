#ifndef SQL_CONNECTION_POOL_H
#define SQL_CONNECTION_POOL_H

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

class connection_pool
{
    struct MysqlCloser
    {
        void operator()(MYSQL* conn) const noexcept
        {
            if(conn)
            {
                mysql_close(conn);
            }
        }
    };

    using MysqlPtr = std::unique_ptr<MYSQL,MysqlCloser>;

public:
	MYSQL* get_connection();
	bool release_connection(MYSQL* conn);
	int get_free_conn() const;

	static connection_pool* get_instance();
	void init(std::string url,std::string user,
            std::string password,std::string dbname,int port,unsigned int maxconn);

	~connection_pool();

    connection_pool(const connection_pool&) = delete;
    connection_pool& operator=(const connection_pool&) = delete;

private:
    connection_pool() = default;

private:

    std::vector<MysqlPtr> m_connections;
    std::queue<MYSQL*> m_free_connections;
    mutable std::mutex m_mutex;
    std::condition_variable m_cond;

    bool m_initialized = false;
    bool m_stopping = false;

    std::string m_url;
	int m_port;
    std::string m_user;
    std::string m_password;
    std::string m_dbname;
};

class connectionRAII
{
public:
	connectionRAII(MYSQL** con,connection_pool* conn_pool);
	~connectionRAII();

    connectionRAII(const connectionRAII&) = delete;
    connectionRAII& operator=(const connectionRAII&) = delete;
private:
	MYSQL* m_conRAII = nullptr;
	connection_pool* m_poolRAII = nullptr;
};




#endif

