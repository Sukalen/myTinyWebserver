#include <cassert>
#include <iostream>
#include <vector>

#include "game_session.h"
#include "level_generator.h"

namespace
{

bool is_solution_cell(const std::vector<game::Position>& solution, int row, int col)
{
    for(const game::Position& cat : solution)
    {
        if(cat.row == row && cat.col == col)
        {
            return true;
        }
    }

    return false;
}


game::Position find_wrong_cell(const game::Level& level)
{
    const int size = level.puzzle.size;

    for(int row = 0; row < size; ++row)
    {
        for(int col = 0; col < size; ++col)
        {
            if(!is_solution_cell(level.solution, row, col))
            {
                return {row, col};
            }
        }
    }

    return {};
}

}


int main()
{
    game::LevelGenerator generator(12345);

    /*
     * V1 暂时使用能够稳定生成的 5 阶。
     */
    game::Level level = generator.generate(5);

    const std::vector<game::Position> solution = level.solution;
    const game::Position wrong = find_wrong_cell(level);

    game::GameSession session(level);

    /*
     * 先故意放一只错误猫。
     */
    auto result = session.set_cell(wrong.row, wrong.col, game::CellState::Cat);

    assert(result == game::GameSession::ActionResult::Success);
    assert(!session.completed());

    /*
     * 再把所有真正的猫放进去。
     *
     * 因为还有一个错误猫，
     * 所以此时仍然不能完成。
     */
    for(const game::Position& cat : solution)
    {
        session.set_cell(cat.row, cat.col, game::CellState::Cat);
    }

    assert(!session.completed());

    /*
     * 清除错误猫。
     *
     * 此时玩家放置的猫集合
     * 正好等于 solution。
     */
    session.set_cell(wrong.row, wrong.col, game::CellState::Unknown);

    assert(session.completed());

    /*
     * 已完成的游戏禁止继续修改。
     */
    result = session.set_cell(0, 0, game::CellState::Excluded);

    assert(result == game::GameSession::ActionResult::AlreadyCompleted);


    /*
     * 单独测试提示。
     */
    game::GameSession hint_session(level);

    game::GameSession::HintResult hint = hint_session.use_hint();

    assert(hint.status == game::GameSession::HintStatus::Success);
    assert(is_solution_cell(solution, hint.position.row, hint.position.col));

    game::GameSession::Snapshot state = hint_session.snapshot();

    assert(state.hint_used == 1);
    assert(state.cells[state.puzzle.index(hint.position.row, hint.position.col)] == game::CellState::Cat);

    std::cout << "GameSession test passed\n";

    return 0;
}
