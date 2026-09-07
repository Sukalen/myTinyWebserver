#include <iostream>

#include "game_rule.h"
#include "game_solver.h"
#include "level_generator.h"

int main()
{
    game::LevelGenerator generator(
        12345);

    for(int size = 5;
        size <= 16;
        ++size)
    {
        try
        {
            game::Level level =
                generator.generate(size);

            auto solution_result =
                game::GameRule::
                    check_solution(
                        level.puzzle,
                        level.solution);

            auto solve_result =
                game::GameSolver::
                    solve(
                        level.puzzle,
                        2);

            std::cout
                << "size = "
                << size
                << '\n';

            std::cout
                << "official solution valid = "
                << solution_result.valid
                << '\n';

            std::cout
                << "solution count = "
                << solve_result.solution_count
                << '\n';

            std::cout
                << "unique = "
                << solve_result.unique()
                << "\n\n";
        }
        catch(const std::exception& e)
        {
            std::cout
                << "size = "
                << size
                << " failed: "
                << e.what()
                << "\n\n";
        }
    }

    return 0;
}
