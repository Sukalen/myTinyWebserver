#ifndef AUTH_SERVICE_H
#define AUTH_SERVICE_H

#include <map>
#include <mutex>
#include <string>

#include "../CGImysql/sql_connection_pool.h"

class AuthService
{
public:
    enum class RegisterResult
    {
        Success,
        AlreadyExists,
        DatabaseError
    };

public:
    explicit AuthService(connection_pool* pool);

    AuthService(const AuthService&) = delete;
    AuthService& operator=(const AuthService&) = delete;

    bool load_users();

    bool login(const std::string& username, const std::string& password) const;

    RegisterResult register_user(const std::string& username, const std::string& password);

private:
 
    connection_pool* m_pool;

    std::map<std::string, std::string> m_users;

    mutable std::mutex m_mutex;
};

#endif
