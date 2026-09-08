#include "game_session.h"

#include <stdexcept>
#include <utility>

#include "game_rule.h"

namespace game
{

GameSession::GameSession(Level level) : m_level(std::move(level))
{
    GameRule::CheckResult result = GameRule::check_solution(m_level.puzzle, m_level.solution);

    if(!result.valid)
    {
        throw std::invalid_argument("GameSession requires a valid level: " + result.message);
    }

    const int size = m_level.puzzle.size;
    const std::size_t total_cells = static_cast<std::size_t>(size) * static_cast<std::size_t>(size);

    m_cells.assign(total_cells, CellState::Unknown);
}


GameSession::ActionResult
GameSession::set_cell(int row, int col, CellState state)
{
    std::lock_guard<std::mutex> lock(m_mutex);

    if(m_completed)
    {
        return ActionResult::AlreadyCompleted;
    }

    if(!valid_position(row, col))
    {
        return ActionResult::InvalidPosition;
    }

    m_cells[m_level.puzzle.index(row, col)] = state;

    update_completed_locked();

    return ActionResult::Success;
}


GameSession::HintResult
GameSession::use_hint()
{
    std::lock_guard<std::mutex> lock(m_mutex);

    if(m_completed)
    {
        return {HintStatus::AlreadyCompleted, {}};
    }

    /*
     * 第一版先选择第一个尚未发现的猫。
     *
     * 以后如果想增加随机性，
     * 再随机选择即可。
     */
    for(const Position& cat : m_level.solution)
    {
        const std::size_t index = m_level.puzzle.index(cat.row, cat.col);

        if(m_cells[index] == CellState::Cat)
        {
            continue;
        }

        m_cells[index] = CellState::Cat;
        ++m_hint_used;

        update_completed_locked();

        return {HintStatus::Success, cat};
    }

    return {HintStatus::NoHintAvailable, {}};
}


GameSession::Snapshot
GameSession::snapshot() const
{
    std::lock_guard<std::mutex> lock(m_mutex);

    Snapshot result;

    result.puzzle = m_level.puzzle;
    result.cells = m_cells;
    result.hint_used = m_hint_used;
    result.completed = m_completed;

    return result;
}


bool GameSession::completed() const
{
    std::lock_guard<std::mutex> lock(m_mutex);

    return m_completed;
}


bool GameSession::valid_position(int row, int col) const noexcept
{
    const int size = m_level.puzzle.size;

    return row >= 0 && row < size && col >= 0 && col < size;
}


void GameSession::update_completed_locked()
{
    m_completed = matches_solution_locked();
}


bool GameSession::matches_solution_locked() const
{
    /*
     * 玩家必须正好放 N 只猫。
     *
     * 这样可以避免：
     *
     * 正确 N 只猫全部找到
     * +
     * 额外放了错误猫
     *
     * 也被判定完成。
     */
    int cat_count = 0;

    for(CellState state : m_cells)
    {
        if(state == CellState::Cat)
        {
            ++cat_count;
        }
    }

    if(cat_count != m_level.puzzle.size)
    {
        return false;
    }

    /*
     * 官方 solution 中的每只猫
     * 都必须被玩家标为 Cat。
     */
    for(const Position& cat : m_level.solution)
    {
        if(m_cells[m_level.puzzle.index(cat.row, cat.col)] != CellState::Cat)
        {
            return false;
        }
    }

    return true;
}

}
