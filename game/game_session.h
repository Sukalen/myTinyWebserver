#ifndef GAME_SESSION_H
#define GAME_SESSION_H

#include <mutex>
#include <vector>

#include "game_types.h"

namespace game
{

class GameSession
{
public:
    enum class ActionResult
    {
        Success,
        InvalidPosition,
        AlreadyCompleted
    };

    enum class HintStatus
    {
        Success,
        NoHintAvailable,
        AlreadyCompleted
    };

    struct HintResult
    {
        HintStatus status = HintStatus::NoHintAvailable;
        Position position;
    };

    /*
     * 给客户端/上层业务使用的状态快照
     */
    struct Snapshot
    {
        Puzzle puzzle;
        std::vector<CellState> cells;
        int hint_used = 0;
        bool completed = false;
    };

public:
    explicit GameSession(Level level);

    GameSession(const GameSession&) = delete;
    GameSession& operator=(const GameSession&) = delete;

    ActionResult set_cell(int row, int col, CellState state);

    HintResult use_hint();

    Snapshot snapshot() const;

    bool completed() const;

private:
    bool valid_position(int row, int col) const noexcept;

    /*
     * 调用这两个函数之前，
     * 必须已经持有 m_mutex。
     */
    void update_completed_locked();
    bool matches_solution_locked() const;

private:
    Level m_level;

    std::vector<CellState> m_cells;

    int m_hint_used = 0;
    bool m_completed = false;

    mutable std::mutex m_mutex;
};

}

#endif
