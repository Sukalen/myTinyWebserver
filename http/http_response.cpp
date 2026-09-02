#include "http_response.h"

#include<utility>


void HttpResponse::reset()
{
	m_status_code = 200;
	m_reason = "OK";

	m_keep_alive = false;

	m_content_length = 0;

	m_content_type.clear();

	m_header.clear();

	m_body.clear();
}


void HttpResponse::set_status(int status_code, std::string reason)
{
	m_status_code = status_code;
	m_reason = std::move(reason);
}

void HttpResponse::set_keep_alive(bool keep_alive)
{
	m_keep_alive = keep_alive;
}

void HttpResponse::set_content_type(std::string content_type)
{
	m_content_type = std::move(content_type);
}

void HttpResponse::set_content_length(std::size_t content_length)
{
	m_content_length = content_length;
}

void HttpResponse::set_body(std::string body)
{
	m_body = std::move(body);
	m_content_length = m_body.size();
}

void HttpResponse::build()
{
	m_header.clear();

	m_header += 
		"HTTP/1.1 " +
        std::to_string(m_status_code) +
        " " +
        m_reason +
        "\r\n";

	m_header +=
        "Content-Length: " +
        std::to_string(m_content_length) +
        "\r\n";

    if(!m_content_type.empty())
    {
        m_header +=
            "Content-Type: " +
            m_content_type +
            "\r\n";
    }

	m_header += "Connection: ";

	m_header += m_keep_alive ? "keep-alive\r\n" : "close\r\n";

	m_header += "\r\n";

}





