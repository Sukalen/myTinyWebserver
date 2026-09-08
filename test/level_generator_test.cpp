#include <iostream>

#include "game_rule.h"
#include "level_generator.h"

int main()
{
    game::LevelGenerator generator(
        12345);

    for(int size = 5;
        size <= 16;
        ++size)
    {
        /*
         * 1. 生成猫。
         */
        std::vector<game::Position>
            cats =
                generator.
                generate_cat_positions(
                    size);

        /*
         * 2. 生成区域。
         */
        std::vector<int> regions =
            generator.generate_regions(
                size,
                cats);

        /*
         * 3. 构造 Puzzle。
         */
        game::Puzzle puzzle;

        puzzle.size = size;
        puzzle.regions =
            std::move(regions);

        /*
         * 4. 检查 Puzzle。
         */
        auto puzzle_result =
            game::GameRule::
                check_puzzle(
                    puzzle);

        /*
         * 5. 检查我们生成的猫
         * 是否是这个 Puzzle 的合法解。
         */
        auto solution_result =
            game::GameRule::
                check_solution(
                    puzzle,
                    cats);

        std::cout
            << "size = "
            << size
            << ", puzzle = "
            << puzzle_result.valid
            << ", solution = "
            << solution_result.valid
            << '\n';
		if(size == 8)
{
    std::cout
        << "\n8x8 region map:\n";

    for(int row = 0;
        row < size;
        ++row)
    {
        for(int col = 0;
            col < size;
            ++col)
        {
            std::cout
                << puzzle.region_at(
                    row,
                    col)
                << ' ';
        }

        std::cout << '\n';
    }
}
    }

    return 0;
}
