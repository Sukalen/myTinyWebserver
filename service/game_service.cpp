#include "game_service.h"

#include <chrono>
#include <iomanip>
#include <sstream>
#include <stdexcept>
#include <utility>

#include "../game/level_generator.h"
#include "../log/log.h"

namespace game
{

bool GameService::supported_size(int size) noexcept
{
    return size >= 5 && size <= 8;
}


const char* GameService::message(Code code) noexcept
{
    switch(code)
    {
        case Code::Success:
            return "ok";

        case Code::InvalidRequest:
            return "invalid request";

        case Code::InvalidPosition:
            return "invalid position";

        case Code::UnsupportedBoardSize:
            return "unsupported board size";

        case Code::SessionNotFound:
            return "session not found";

        case Code::GameAlreadyCompleted:
            return "game already completed";

        case Code::NoHintAvailable:
            return "no hint available";

        case Code::LevelGenerationFailed:
            return "level generation failed";

        case Code::InternalServerError:
            return "internal server error";
    }

    return "internal server error";
}


std::string GameService::generate_session_id()
{
    const auto now = std::chrono::system_clock::now().time_since_epoch();

    const auto timestamp =
        std::chrono::duration_cast<std::chrono::microseconds>(now).count();

    unsigned long long sequence = 0;

    {
        std::lock_guard<std::mutex> lock(m_mutex);
        sequence = m_next_session_id++;
    }

    std::ostringstream stream;

    stream << std::hex
           << timestamp
           << '-'
           << sequence;

    return stream.str();
}

std::shared_ptr<GameSession>
GameService::find_session(const std::string& session_id) const
{
    std::lock_guard<std::mutex> lock(m_mutex);

    auto it = m_sessions.find(session_id);

    if(it == m_sessions.end())
    {
        return nullptr;
    }

    return it->second;
}

GameService::StateResult
GameService::start_game(int size)
{
    StateResult result;

    if(!supported_size(size))
    {
        result.code = Code::UnsupportedBoardSize;
        return result;
    }

    try
    {
        /*
         * 每次创建游戏使用一个独立 Generator。
         *
         * 避免多个 worker 共享同一个 mt19937
         * 产生数据竞争。
         */
        LevelGenerator generator;

        Level level = generator.generate(size);

        auto session = std::make_shared<GameSession>(std::move(level));

        const std::string session_id = generate_session_id();

        {
            std::lock_guard<std::mutex> lock(m_mutex);
            m_sessions.emplace(session_id, session);
        }

        result.code = Code::Success;
        result.session_id = session_id;
        result.state = session->snapshot();

        return result;
    }
    catch(const std::runtime_error& e)
    {
        LOG_ERROR("level generation failed: %s", e.what());

        result.code = Code::LevelGenerationFailed;
        return result;
    }
    catch(const std::exception& e)
    {
        LOG_ERROR("start game failed: %s", e.what());

        result.code = Code::InternalServerError;
        return result;
    }
}


GameService::StateResult
GameService::action(
    const std::string& session_id,
    int row,
    int col,
    CellState state)
{
    StateResult result;
    result.session_id = session_id;

    std::shared_ptr<GameSession> session = find_session(session_id);

    if(!session)
    {
        result.code = Code::SessionNotFound;
        return result;
    }

    GameSession::ActionResult action_result = session->set_cell(row, col, state);

    switch(action_result)
    {
        case GameSession::ActionResult::Success:
            result.code = Code::Success;
            result.state = session->snapshot();
            return result;

        case GameSession::ActionResult::InvalidPosition:
            result.code = Code::InvalidPosition;
            return result;

        case GameSession::ActionResult::AlreadyCompleted:
            result.code = Code::GameAlreadyCompleted;
            return result;
    }

    result.code = Code::InternalServerError;
    return result;
}


GameService::HintResult
GameService::hint(const std::string& session_id)
{
    HintResult result;
    result.session_id = session_id;

    std::shared_ptr<GameSession> session = find_session(session_id);

    if(!session)
    {
        result.code = Code::SessionNotFound;
        return result;
    }

    GameSession::HintResult hint_result = session->use_hint();

    switch(hint_result.status)
    {
        case GameSession::HintStatus::Success:
            result.code = Code::Success;
            result.position = hint_result.position;
            result.state = session->snapshot();
            return result;

        case GameSession::HintStatus::NoHintAvailable:
            result.code = Code::NoHintAvailable;
            return result;

        case GameSession::HintStatus::AlreadyCompleted:
            result.code = Code::GameAlreadyCompleted;
            return result;
    }

    result.code = Code::InternalServerError;
    return result;
}


GameService::StateResult
GameService::get_state(const std::string& session_id) const
{
    StateResult result;
    result.session_id = session_id;

    std::shared_ptr<GameSession> session = find_session(session_id);

    if(!session)
    {
        result.code = Code::SessionNotFound;
        return result;
    }

    result.code = Code::Success;
    result.state = session->snapshot();

    return result;
}

std::size_t GameService::session_count() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_sessions.size();
}

}
