#ifndef ROUTER_H
#define ROUTER_H

#include<string>

#include "http_request.h"

class Router
{
public:
	enum class RouteType
	{
		StaticFile,
		Login,
		Register
	};

	struct RouteResult
	{
		RouteType type = RouteType::StaticFile;

		std::string target;
	};

public:
	Router() = default;

	RouteResult resolve(const HttpRequest& request) const;

private:
	char legacy_route_code(const std::string& url) const;

};






#endif
