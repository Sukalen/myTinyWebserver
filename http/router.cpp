#include "router.h"

Router::RouteResult
Router::resolve(const HttpRequest& request) const
{
    const std::string& url = request.url();
    RouteResult result;

    result.type = RouteType::StaticFile;
    result.target = url;

    const char route_code = legacy_route_code(url);


    if(HttpRequest::Method::Post == request.method())
    {
        if('2' == route_code)
        {
            result.type = RouteType::Login;
            return result;
        }

        if('3' == route_code)
        {
            result.type = RouteType::Register;
            return result;
        }
    }

    switch(route_code)
    {
        case '0':
            result.target = "/register.html";
            break;

        case '1':
            result.target = "/log.html";
            break;

        case '5':
            result.target = "/picture.html";
            break;

        case '6':
            result.target = "/video.html";
            break;

        default:
            break;
    }

    return result;
}

char Router::legacy_route_code(const std::string& url) const
{
    const std::size_t slash = url.find_last_of('/');

    if(std::string::npos ==slash ||
       slash + 1 >= url.size())
    {
        return '\0';
    }

    return url[slash + 1];
}


