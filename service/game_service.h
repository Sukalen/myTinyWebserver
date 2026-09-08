#ifndef GAME_SERVICE_H
#define GAME_SERVICE_H

#include <cstddef>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>

#include "../game/game_session.h"

namespace game
{

class GameService
{
public:
    /*
     * 数值严格对应 docs/api.md。
     */
    enum class Code
    {
        Success = 0,

        InvalidRequest = 1001,
        InvalidPosition = 1002,
        UnsupportedBoardSize = 1003,

        SessionNotFound = 2001,
        GameAlreadyCompleted = 2002,
        NoHintAvailable = 2003,

        LevelGenerationFailed = 3001,

        InternalServerError = 5000
    };

    struct StateResult
    {
        Code code = Code::InternalServerError;
        std::string session_id;
        GameSession::Snapshot state;

        bool success() const noexcept
        {
            return code == Code::Success;
        }
    };

    struct HintResult
    {
        Code code = Code::InternalServerError;
        std::string session_id;
        Position position;
        GameSession::Snapshot state;

        bool success() const noexcept
        {
            return code == Code::Success;
        }
    };

public:
    GameService() = default;

    GameService(const GameService&) = delete;
    GameService& operator=(const GameService&) = delete;

    StateResult start_game(int size);

    StateResult action(
        const std::string& session_id,
        int row,
        int col,
        CellState state);

    HintResult hint(const std::string& session_id);

    StateResult get_state(const std::string& session_id) const;

    std::size_t session_count() const;

    static const char* message(Code code) noexcept;

private:
    static bool supported_size(int size) noexcept;

    std::shared_ptr<GameSession> find_session(const std::string& session_id) const;

    std::string generate_session_id();

private:
    mutable std::mutex m_mutex;

    std::unordered_map<
        std::string,
        std::shared_ptr<GameSession>>
        m_sessions;

    unsigned long long m_next_session_id = 1;
};

}

#endif
