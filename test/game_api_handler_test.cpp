#include <cassert>
#include <iostream>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

#include "../http/game_api_handler.h"

using json = nlohmann::json;

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

std::string make_post(const std::string& path, const std::string& body)
{
    return "POST " + path + " HTTP/1.1\r\n"
           "Host: localhost\r\n"
           "Content-Type: application/json\r\n"
           "Content-Length: " + std::to_string(body.size()) + "\r\n"
           "\r\n" +
           body;
}

}

int main()
{
    game::GameService service;
    GameApiHandler handler(&service);
    Router router;

    /*
     * start
     */
    const std::string start_body = R"({"size":5})";

    HttpRequest start_request =
        parse_request(make_post("/api/game/start", start_body));

    Router::RouteResult start_route = router.resolve(start_request);

    std::string start_response =
        handler.handle(start_request, start_route.type);

    json start_json = json::parse(start_response);

    assert(start_json["code"] == 0);
    assert(start_json["data"]["size"] == 5);
    assert(start_json["data"]["regions"].size() == 25);
    assert(start_json["data"]["cells"].size() == 25);
    assert(!start_json["data"].contains("solution"));

    const std::string session_id =
        start_json["data"]["sessionId"].get<std::string>();


    /*
     * action
     */
    json action_body = {
        {"sessionId", session_id},
        {"row", 0},
        {"col", 0},
        {"state", "excluded"}
    };

    HttpRequest action_request =
        parse_request(
            make_post(
                "/api/game/action",
                action_body.dump()));

    Router::RouteResult action_route =
        router.resolve(action_request);

    json action_response =
        json::parse(
            handler.handle(
                action_request,
                action_route.type));

    assert(action_response["code"] == 0);


    /*
     * state
     */
    HttpRequest state_request =
        parse_request(
            "GET /api/game/state?sessionId=" +
            session_id +
            " HTTP/1.1\r\n"
            "Host: localhost\r\n"
            "\r\n");

    Router::RouteResult state_route =
        router.resolve(state_request);

    json state_response =
        json::parse(
            handler.handle(
                state_request,
                state_route.type));

    assert(state_response["code"] == 0);
    assert(state_response["data"]["sessionId"] == session_id);


    /*
     * hint
     */
    json hint_body = {
        {"sessionId", session_id}
    };

    HttpRequest hint_request =
        parse_request(
            make_post(
                "/api/game/hint",
                hint_body.dump()));

    Router::RouteResult hint_route =
        router.resolve(hint_request);

    json hint_response =
        json::parse(
            handler.handle(
                hint_request,
                hint_route.type));

    assert(hint_response["code"] == 0);
    assert(hint_response["data"].contains("position"));
    assert(hint_response["data"].contains("game"));
    assert(hint_response["data"]["game"]["hintUsed"] == 1);


    /*
     * invalid JSON
     */
    HttpRequest invalid_request =
        parse_request(
            make_post(
                "/api/game/start",
                "{bad json}"));

    Router::RouteResult invalid_route =
        router.resolve(invalid_request);

    json invalid_response =
        json::parse(
            handler.handle(
                invalid_request,
                invalid_route.type));

    assert(invalid_response["code"] == 1001);

    std::cout << "GameApiHandler test passed\n";

    return 0;
}
