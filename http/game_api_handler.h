#ifndef GAME_API_HANDLER_H
#define GAME_API_HANDLER_H

#include <string>

#include "http_request.h"
#include "router.h"
#include "../service/game_service.h"

class GameApiHandler
{
public:
    explicit GameApiHandler(game::GameService* game_service);

    GameApiHandler(const GameApiHandler&) = delete;
    GameApiHandler& operator=(const GameApiHandler&) = delete;

    std::string handle(const HttpRequest& request, Router::RouteType route_type) const;

private:
    std::string handle_start(const HttpRequest& request) const;
    std::string handle_action(const HttpRequest& request) const;
    std::string handle_hint(const HttpRequest& request) const;
    std::string handle_state(const HttpRequest& request) const;

private:
    game::GameService* m_game_service;
};

#endif
