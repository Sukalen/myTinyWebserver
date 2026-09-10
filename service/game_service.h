#ifndef GAME_SERVICE_H
#define GAME_SERVICE_H

#include <cstddef>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <chrono>

#include "../game/game_session.h"

namespace game
{

class GameService
{

private:
	struct SessionEntry
	{
    	std::shared_ptr<GameSession> session;
    	std::chrono::steady_clock::time_point last_active;
	};

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
	explicit GameService(std::chrono::seconds session_timeout = std::chrono::minutes(30));

    GameService(const GameService&) = delete;
    GameService& operator=(const GameService&) = delete;

    StateResult start_game(int size);

    StateResult action(
        const std::string& session_id,
        int row,
        int col,
        CellState state);

    HintResult hint(const std::string& session_id);

    StateResult get_state(const std::string& session_id);

    std::size_t session_count() const;

    static const char* message(Code code) noexcept;

	std::size_t cleanup_expired_sessions();

private:
    static bool supported_size(int size) noexcept;

    std::shared_ptr<GameSession> find_session(const std::string& session_id);

    std::string generate_session_id();

	static std::chrono::minutes session_timeout() noexcept;

private:
    mutable std::mutex m_mutex;

    std::unordered_map<std::string, SessionEntry> m_sessions;

    unsigned long long m_next_session_id = 1;

	std::chrono::seconds m_session_timeout;
};

}

#endif
