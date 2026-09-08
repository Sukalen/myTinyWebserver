#include <cassert>
#include <iostream>
#include <string>
#include <vector>

#include "../http/http_request.h"
#include "../http/router.h"

namespace
{

HttpRequest parse_request(const std::string& raw)
{
    HttpRequest request;

    std::vector<char> buffer(raw.begin(), raw.end());

    HttpRequest::ParseResult result = request.parse(buffer.data(), buffer.size());

    assert(result == HttpRequest::ParseResult::Complete);

    return request;
}

}

int main()
{
    Router router;

    /*
     * start
     */
    {
        HttpRequest request = parse_request(
            "POST /api/game/start HTTP/1.1\r\n"
            "Host: localhost\r\n"
            "\r\n");

        Router::RouteResult route = router.resolve(request);

        assert(request.path() == "/api/game/start");
        assert(route.type == Router::RouteType::GameStart);
    }


    /*
     * action
     */
    {
        HttpRequest request = parse_request(
            "POST /api/game/action HTTP/1.1\r\n"
            "Host: localhost\r\n"
            "\r\n");

        Router::RouteResult route = router.resolve(request);

        assert(route.type == Router::RouteType::GameAction);
    }


    /*
     * hint
     */
    {
        HttpRequest request = parse_request(
            "POST /api/game/hint HTTP/1.1\r\n"
            "Host: localhost\r\n"
            "\r\n");

        Router::RouteResult route = router.resolve(request);

        assert(route.type == Router::RouteType::GameHint);
    }


    /*
     * state + query
     */
    {
        HttpRequest request = parse_request(
            "GET /api/game/state?sessionId=abc-123 HTTP/1.1\r\n"
            "Host: localhost\r\n"
            "\r\n");

        Router::RouteResult route = router.resolve(request);

        assert(request.url() == "/api/game/state?sessionId=abc-123");
        assert(request.path() == "/api/game/state");
        assert(request.query() == "sessionId=abc-123");

        std::string session_id;

        assert(request.query_param("sessionId", session_id));
        assert(session_id == "abc-123");

        assert(route.type == Router::RouteType::GameState);
    }


    /*
     * 方法错误。
     */
    {
        HttpRequest request = parse_request(
            "GET /api/game/start HTTP/1.1\r\n"
            "Host: localhost\r\n"
            "\r\n");

        Router::RouteResult route = router.resolve(request);

        assert(route.type == Router::RouteType::InvalidApi);
    }


    /*
     * 未知 API。
     */
    {
        HttpRequest request = parse_request(
            "GET /api/game/not-exist HTTP/1.1\r\n"
            "Host: localhost\r\n"
            "\r\n");

        Router::RouteResult route = router.resolve(request);

        assert(route.type == Router::RouteType::InvalidApi);
    }


    /*
     * 静态文件 query 不应进入文件路径。
     */
    {
        HttpRequest request = parse_request(
            "GET /picture.html?v=123 HTTP/1.1\r\n"
            "Host: localhost\r\n"
            "\r\n");

        Router::RouteResult route = router.resolve(request);

        assert(route.type == Router::RouteType::StaticFile);
        assert(route.target == "/picture.html");
    }

    std::cout << "HttpRequest/Router test passed\n";

    return 0;
}
