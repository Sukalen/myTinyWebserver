#include <iostream>

#include "game_rule.h"
#include "game_solver.h"
int main()
{
    game::Puzzle puzzle;

    puzzle.size = 5;

    /*
     * 为了第一轮测试简单，
     * 每一行暂时作为一个区域。
     *
     * 真正 Generator 不会这样生成。
     */
    puzzle.regions =
    {
        0, 0, 0, 0, 0,
        1, 1, 1, 1, 1,
        2, 2, 2, 2, 2,
        3, 3, 3, 3, 3,
        4, 4, 4, 4, 4
    };

    /*
     * col：
     *
     * 0, 2, 4, 1, 3
     *
     * 相邻两行猫的列差都 > 1。
     */
    std::vector<game::Position>
        valid_solution =
    {
        {0, 0},
        {1, 2},
        {2, 4},
        {3, 1},
        {4, 3}
    };

    auto puzzle_result =
        game::GameRule::check_puzzle(
            puzzle);

    std::cout
        << "puzzle: "
        << puzzle_result.valid
        << " "
        << puzzle_result.message
        << '\n';

    auto solution_result =
        game::GameRule::check_solution(
            puzzle,
            valid_solution);

    std::cout
        << "solution: "
        << solution_result.valid
        << " "
        << solution_result.message
        << '\n';


    /*
     * 故意制造错误：
     *
     * 猫沿主对角线排列，
     * 相邻猫发生对角接触。
     */
    std::vector<game::Position>
        invalid_solution =
    {
        {0, 0},
        {1, 1},
        {2, 2},
        {3, 3},
        {4, 4}
    };

    auto invalid_result =
        game::GameRule::check_solution(
            puzzle,
            invalid_solution);

    std::cout
        << "invalid solution: "
        << invalid_result.valid
        << " "
        << invalid_result.message
        << '\n';


	auto solve_result =
    game::GameSolver::solve(
        puzzle);

std::cout
    << "solver valid: "
    << solve_result.valid_puzzle
    << '\n';

std::cout
    << "solution count: "
    << solve_result.solution_count
    << '\n';

std::cout
    << "message: "
    << solve_result.message
    << '\n';

std::cout
    << "first solution:"
    << '\n';

for(const auto& cat :
    solve_result.first_solution)
{
    std::cout
        << "("
        << cat.row
        << ", "
        << cat.col
        << ")"
        << '\n';
}

    return 0;
}
