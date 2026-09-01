#include "http_request.h"

#include<string>
#include<cctype>
#include<algorithm>
#include<sstream>


namespace
{

std::string trim(std::string value)
{
    auto first =
        std::find_if( value.begin(), value.end(),
            [](unsigned char ch){ return !std::isspace(ch);});

    auto last =
        std::find_if(value.rbegin(), value.rend(),
            [](unsigned char ch){ return !std::isspace(ch);}).base();

    if(first >= last)
    {
        return {};
    }

    return std::string(first, last);
}

std::string to_lower(std::string value)
{
	std::transform(value.begin(), value.end(), value.begin(), 
			[](unsigned char ch){ return static_cast<char>(std::tolower(ch));});

	return value;
}
}




void HttpRequest::reset()
{
	m_state = CheckState::RequestLine;

	m_method = Method::Unknown;

    m_checked_idx = 0;
    m_start_line = 0;
    m_content_length = 0;

    m_url.clear();
    m_version.clear();
    m_host.clear();
    m_body.clear();

    m_keep_alive = false;
}

HttpRequest::LineStatus
HttpRequest::parse_line(char* buffer, std::size_t read_size)
{
    for(;m_checked_idx < read_size; ++m_checked_idx)
    {
        char temp = buffer[m_checked_idx];

        if('\r' == temp)
        {
            if(m_checked_idx + 1 == read_size)
            {
                return LineStatus::Open;
            }

            if('\n' == buffer[m_checked_idx + 1])
            {
                buffer[m_checked_idx++] = '\0';

                buffer[m_checked_idx++] = '\0';

                return LineStatus::Ok;
            }

            return LineStatus::Bad;
        }

        if('\n' == temp)
        {
            if(m_checked_idx > 0 && '\r' == buffer[m_checked_idx - 1])
            {
                buffer[m_checked_idx - 1] ='\0';
                buffer[m_checked_idx++] = '\0';

                return LineStatus::Ok;
            }
            return LineStatus::Bad;
        }
    }
    return LineStatus::Open;
}


bool HttpRequest::parse_request_line(char* text)
{
    if(!text)
    {
        return false;
    }

    std::istringstream stream(text);

    std::string method;

    if(!(stream >> method >> m_url >> m_version))
    {
        return false;
    }

    std::string extra;

    if(stream >> extra)
    {
        return false;
    }

    if("GET" == method)
    {
        m_method = Method::Get;
    }
    else if("POST" == method)
    {
        m_method = Method::Post;
    }
    else
    {
        return false;
    }

    if(m_version != "HTTP/1.1")
    {
        return false;
    }

    if( 0 == m_url.rfind("http://", 0) ||
       0 == m_url.rfind("https://", 0))
    {
        std::size_t scheme = m_url.find("://");
        std::size_t path = m_url.find('/', scheme + 3);

        if(std::string::npos == path)
        {
            return false;
        }

        m_url = m_url.substr(path);
    }

    if(m_url.empty() || m_url.front() != '/')
    {
        return false;
    }

    if("/" == m_url)
    {
        m_url = "/judge.html";
    }

    return true;
}


HttpRequest::HeaderResult
HttpRequest::parse_header(char* text)
{
    if(!text)
    {
        return HeaderResult::BadRequest;
    }

    if('\0' == text[0])
    {
        if(m_content_length > 0)
        {
            m_state = CheckState::Content;

            return HeaderResult::Continue;
        }

        return HeaderResult::Complete;
    }

    std::string header(text);

    std::size_t colon = header.find(':');

    if(std::string::npos == colon)
    {
        return HeaderResult::Continue;
    }

    std::string name = to_lower(
		   	trim(
				header.substr(0, colon)));

    std::string value = trim(
		   	header.substr(colon + 1));

    if("connection" == name)
    {
        m_keep_alive =
            "keep-alive" == to_lower(value);
    }

    else if("content-length" == name)
    {
		try
		{
			std::size_t pos = 0;
			unsigned long content_length = std::stoul(value, &pos);

			if(pos != value.size())
			{
				return HeaderResult::BadRequest;
			}

       		m_content_length = static_cast<std::size_t>(content_length);
		}
		catch(const std::exception&)
		{
			return HeaderResult::BadRequest;
		}
    }

    else if("host" == name)
    {
        m_host = value;
    }

    return HeaderResult::Continue;
}


HttpRequest::ParseResult
HttpRequest::parse(char* buffer, std::size_t read_size)
{
    if(!buffer)
    {
        return ParseResult::BadRequest;
    }

    while(true)
    {
        if(CheckState::Content == m_state)
        {
            if(m_start_line > read_size)
            {
                return ParseResult::BadRequest;
            }

            if(m_content_length > read_size - m_start_line)
            {
                return ParseResult::Incomplete;
            }

            m_body.assign(buffer + m_start_line, m_content_length);
            return ParseResult::Complete;
        }

        LineStatus line_status = parse_line(buffer, read_size);

        if(LineStatus::Open == line_status)
        {
            return ParseResult::Incomplete;
        }

        if(LineStatus::Bad == line_status)
        {
            return ParseResult::BadRequest;
        }

        char* text = buffer + m_start_line;

        m_start_line = m_checked_idx;

        if(CheckState::RequestLine == m_state)
        {
            if(!parse_request_line(text))
            {
                return ParseResult::BadRequest;
            }

            m_state = CheckState::Header;
            continue;
        }

        HeaderResult result = parse_header(text);

        if(HeaderResult::BadRequest == result)
        {
            return ParseResult::BadRequest;
        }

        if(HeaderResult::Complete == result)
        {
            return ParseResult::Complete;
        }
    }
}



