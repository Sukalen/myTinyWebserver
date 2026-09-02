#ifndef HTTP_RESPONSE_H
#define HTTP_RESPONSE_H

#include<cstddef>
#include<string>

class HttpResponse
{
public:
	HttpResponse() = default;

	void reset();

	void set_status(int status_code, std::string reason);

	void set_keep_alive(bool keep_alive);

	void set_content_type(std::string content_type);

	void set_content_length(std::size_t content_length);

	void set_body(std::string body);

	void build();

	const std::string& header() const noexcept
	{
		return m_header;
	}

	const std::string& body() const noexcept
	{
		return m_body;
	}

	
private:
	int m_status_code = 200;

	std::string m_reason = "OK";

	bool m_keep_alive = false;

	std::size_t m_content_length = 0;

	std::string m_content_type;
	std::string m_header;
	std::string m_body;

};

#endif
