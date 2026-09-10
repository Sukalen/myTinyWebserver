#include "game_api_handler.h"

#include <stdexcept>
#include <string>

#include <nlohmann/json.hpp>

using json = nlohmann::json;

namespace
{

const char* cell_state_to_string(game::CellState state)
{
    switch(state)
    {
        case game::CellState::Unknown:
            return "unknown";

        case game::CellState::Cat:
            return "cat";

        case game::CellState::Excluded:
            return "excluded";
    }

    return "unknown";
}

bool parse_cell_state(const std::string& value, game::CellState& state)
{
    if(value == "unknown")
    {
        state = game::CellState::Unknown;
        return true;
    }

    if(value == "cat")
    {
        state = game::CellState::Cat;
        return true;
    }

    if(value == "excluded")
    {
        state = game::CellState::Excluded;
        return true;
    }

    return false;
}


json snapshot_to_json(
    const std::string& session_id,
    const game::GameSession::Snapshot& snapshot)
{
    json cells = json::array();

    for(game::CellState state : snapshot.cells)
    {
        cells.push_back(cell_state_to_string(state));
    }

    return {
        {"sessionId", session_id},
        {"size", snapshot.puzzle.size},
        {"regions", snapshot.puzzle.regions},
        {"cells", std::move(cells)},
        {"hintUsed", snapshot.hint_used},
        {"completed", snapshot.completed}
    };
}


std::string make_response(game::GameService::Code code, json data = nullptr)
{
    json response = {
        {"code", static_cast<int>(code)},
        {"message", game::GameService::message(code)},
        {"data", std::move(data)}
    };

    return response.dump();
}

}


GameApiHandler::GameApiHandler(game::GameService* game_service) : m_game_service(game_service)
{
    if(!m_game_service)
    {
        throw std::invalid_argument("GameApiHandler requires GameService");
    }
}


std::string GameApiHandler::handle(
    const HttpRequest& request,
    Router::RouteType route_type) const
{
    switch(route_type)
    {
        case Router::RouteType::GameStart:
            return handle_start(request);

        case Router::RouteType::GameAction:
            return handle_action(request);

        case Router::RouteType::GameHint:
            return handle_hint(request);

        case Router::RouteType::GameState:
            return handle_state(request);

        case Router::RouteType::InvalidApi:
            return make_response(game::GameService::Code::InvalidRequest);

        default:
            return make_response(game::GameService::Code::InvalidRequest);
    }
}


std::string GameApiHandler::handle_start(const HttpRequest& request) const
{
    try
    {
        json body = json::parse(request.body());

        if(!body.is_object() ||
           !body.contains("size") ||
           !body["size"].is_number_integer())
        {
            return make_response(game::GameService::Code::InvalidRequest);
        }

        const int size = body["size"].get<int>();

        game::GameService::StateResult result = m_game_service->start_game(size);

        if(!result.success())
        {
            return make_response(result.code);
        }

        return make_response(
            result.code,
            snapshot_to_json(result.session_id, result.state));
    }
    catch(const json::exception&)
    {
        return make_response(game::GameService::Code::InvalidRequest);
    }
    catch(const std::exception&)
    {
        return make_response(game::GameService::Code::InternalServerError);
    }
}


std::string GameApiHandler::handle_action(const HttpRequest& request) const
{
    try
    {
        json body = json::parse(request.body());

        if(!body.is_object() ||
           !body.contains("sessionId") ||
           !body.contains("row") ||
           !body.contains("col") ||
           !body.contains("state"))
        {
            return make_response(game::GameService::Code::InvalidRequest);
        }

        if(!body["sessionId"].is_string() ||
           !body["row"].is_number_integer() ||
           !body["col"].is_number_integer() ||
           !body["state"].is_string())
        {
            return make_response(game::GameService::Code::InvalidRequest);
        }

        const std::string session_id = body["sessionId"].get<std::string>();
        const int row = body["row"].get<int>();
        const int col = body["col"].get<int>();
        const std::string state_string = body["state"].get<std::string>();

        if(session_id.empty())
        {
            return make_response(game::GameService::Code::InvalidRequest);
        }

        game::CellState state;

        if(!parse_cell_state(state_string, state))
        {
            return make_response(game::GameService::Code::InvalidRequest);
        }

        game::GameService::StateResult result =
            m_game_service->action(session_id, row, col, state);

        if(!result.success())
        {
            return make_response(result.code);
        }

        return make_response(
            result.code,
            snapshot_to_json(result.session_id, result.state));
    }
    catch(const json::exception&)
    {
        return make_response(game::GameService::Code::InvalidRequest);
    }
    catch(const std::exception&)
    {
        return make_response(game::GameService::Code::InternalServerError);
    }
}

std::string GameApiHandler::handle_hint(const HttpRequest& request) const
{
    try
    {
        json body = json::parse(request.body());

        if(!body.is_object() ||
           !body.contains("sessionId") ||
           !body["sessionId"].is_string())
        {
            return make_response(game::GameService::Code::InvalidRequest);
        }

        const std::string session_id = body["sessionId"].get<std::string>();

        if(session_id.empty())
        {
            return make_response(game::GameService::Code::InvalidRequest);
        }

        game::GameService::HintResult result = m_game_service->hint(session_id);

        if(!result.success())
        {
            return make_response(result.code);
        }

        json data = {
            {
                "position",
                {
                    {"row", result.position.row},
                    {"col", result.position.col}
                }
            },
            {
                "game",
                snapshot_to_json(result.session_id, result.state)
            }
        };

        return make_response(result.code, std::move(data));
    }
    catch(const json::exception&)
    {
        return make_response(game::GameService::Code::InvalidRequest);
    }
    catch(const std::exception&)
    {
        return make_response(game::GameService::Code::InternalServerError);
    }
}

std::string GameApiHandler::handle_state(const HttpRequest& request) const
{
    std::string session_id;

    if(!request.query_param("sessionId", session_id) || session_id.empty())
    {
        return make_response(game::GameService::Code::InvalidRequest);
    }

    game::GameService::StateResult result = m_game_service->get_state(session_id);

    if(!result.success())
    {
        return make_response(result.code);
    }

    return make_response(
        result.code,
        snapshot_to_json(result.session_id, result.state));
}


