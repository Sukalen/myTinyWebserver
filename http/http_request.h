#ifndef HTTP_REQUEST_H
#define HTTP_REQUEST_H

#include <cstddef>
#include <string>

class HttpRequest
{
public:
    enum class Method
    {
        Get,
        Post,
        Unknown
    };

    enum class ParseResult
    {
        Incomplete,
        Complete,
        BadRequest
    };

public:
    HttpRequest() = default;

    void reset();

    ParseResult parse(
        char* buffer,
        std::size_t read_size);

    Method method() const noexcept
    {
        return m_method;
    }

    const std::string& url() const noexcept
    {
        return m_url;
    }

    const std::string& version() const noexcept
    {
        return m_version;
    }

    const std::string& host() const noexcept
    {
        return m_host;
    }

    const std::string& body() const noexcept
    {
        return m_body;
    }

    bool keep_alive() const noexcept
    {
        return m_keep_alive;
    }

private:
    enum class CheckState
    {
        RequestLine,
        Header,
        Content
    };

    enum class LineStatus
    {
        Ok,
        Bad,
        Open
    };

    enum class HeaderResult
    {
        Continue,
        Complete,
        BadRequest
    };

private:
    LineStatus parse_line(
        char* buffer,
        std::size_t read_size);

    bool parse_request_line(char* text);

    HeaderResult parse_header(char* text);

private:
    CheckState m_state =
        CheckState::RequestLine;

    Method m_method =
        Method::Unknown;

    std::size_t m_checked_idx = 0;
    std::size_t m_start_line = 0;
    std::size_t m_content_length = 0;

    std::string m_url;
    std::string m_version;
    std::string m_host;
    std::string m_body;

    bool m_keep_alive = false;
};

#endif
