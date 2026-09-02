#include<memory>
#include<stdexcept>

#include "auth_service.h"
#include "../log/log.h"

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


AuthService::AuthService(connection_pool* pool):m_pool(pool)
{
    if(!m_pool)
    {
        throw std::invalid_argument(
            "AuthService requires connection pool");
    }
}

bool AuthService::load_users()
{
    try
    {
        MYSQL* mysql = nullptr;
        connectionRAII mysqlcon(&mysql, m_pool);

        if(mysql_query(
                mysql,
                "SELECT username,passwd "
                "FROM user") != 0)
        {
            LOG_ERROR(
                "SELECT user failed: %s",
                mysql_error(mysql));

            return false;
        }

        MysqlResultPtr result(mysql_store_result(mysql));

        if(!result)
        {
            LOG_ERROR(
                "mysql_store_result "
                "failed: %s",
                mysql_error(mysql));

            return false;
        }

        std::map<std::string, std::string> loaded_users;

        while(MYSQL_ROW row =
                mysql_fetch_row(result.get()))
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

        return true;
    }

    catch(const std::exception& e)
    {
        LOG_ERROR(
            "load users failed: %s",
            e.what());

        return false;
    }
}


bool AuthService::login(
    const std::string& username,
    const std::string& password) const
{
    std::lock_guard<std::mutex> lock(m_mutex);

    auto it = m_users.find(username);

    return it != m_users.end() &&
        it->second == password;
}


AuthService::RegisterResult
AuthService::register_user(
    const std::string& username,
    const std::string& password)
{

    std::lock_guard<std::mutex> lock(m_mutex);

    if(m_users.find(username) != m_users.end())
    {
        return RegisterResult::AlreadyExists;
    }

    try
    {
        MYSQL* mysql = nullptr;

        connectionRAII mysqlcon(&mysql, m_pool);

        std::string sql_insert =
            "INSERT INTO "
            "user(username, passwd) "
            "VALUES('" +
            username +
            "', '" +
            password +
            "')";

        int result = mysql_query(mysql,sql_insert.c_str());

        if(result != 0)
        {
            LOG_ERROR(
                "insert user failed: %s",
                mysql_error(mysql));

            return RegisterResult::DatabaseError;
        }

        m_users.emplace(username, password);

        return RegisterResult::Success;
    }

    catch(const std::exception& e)
    {
        LOG_ERROR(
            "register user failed: %s",
            e.what());

        return RegisterResult::DatabaseError;
    }
}



