#include "game_solver.h"

#include "game_rule.h"

namespace game
{

GameSolver::SolveResult 
GameSolver::solve(const Puzzle& puzzle, int limit)
{
	SolveResult result;

    GameRule::CheckResult puzzle_result =
        GameRule::check_puzzle(puzzle);

    if(!puzzle_result.valid)
    {
        result.valid_puzzle = false;
        result.message = puzzle_result.message;

        return result;
    }

    if(limit <= 0)
    {
        result.valid_puzzle = false;
        result.message = "solution limit must be positive";

        return result;
    }

    result.valid_puzzle = true;
	
	//cat_columns[row] = col  表示第row行的猫放在第col列
	std::vector<int> cat_columns(static_cast<std::size_t>(puzzle.size), -1);

	dfs(puzzle,
		0,
		limit,
		0,
		0,
		cat_columns,
		result);

	if(0 == result.solution_count)
	{
		result.message = "puzzle has no solution";
	}
	else if(1 == result.solution_count)
	{
		result.message = "puzzle has unique solution";
	}
	else
	{
		result.message = "puzzle has multiple solutions";
	}

	return result;
}

void GameSolver::dfs(
		const Puzzle& puzzle,
		int row,
		int limit,
		std::uint32_t used_columns,
		std::uint32_t used_regions,
		std::vector<int>& cat_columns,
		SolveResult& result)
{
	if(result.solution_count >= limit)
	{
		return;
	}

	const int size = puzzle.size;

	if(row == size)
	{
		record_solution(
				puzzle,
				cat_columns,
				result);
		return;
	}


	//当前行尝试每一列
	for(int col = 0; col < size; ++col)
	{
		if(!can_place(
					puzzle,
					row,
					col,
					used_columns,
					used_regions,
					cat_columns))
		{
			continue;
		}

		const int region = puzzle.region_at(row, col);

		const std::uint32_t column_bit = std::uint32_t{1} << col;

        const std::uint32_t region_bit = std::uint32_t{1} << region;

		cat_columns[static_cast<std::size_t>(row)] = col;

		dfs(
			puzzle,
			row + 1,
			limit,
			used_columns | column_bit,
			used_regions | region_bit,
			cat_columns,
			result);


		//回溯，bitmask是按值传递，不需要主动恢复
		cat_columns[static_cast<std::size_t>(row)] = -1;

		//发现多个解，退出
		if(result.solution_count >= limit)
		{
			return;
		}
	}

}


bool GameSolver::can_place(
		const Puzzle& puzzle,
    	int row,
    	int col,
    	std::uint32_t used_columns,
    	std::uint32_t used_regions,
    	const std::vector<int>& cat_columns)
{
	const std::uint32_t column_bit = std::uint32_t{1} << col;

	if(used_columns & column_bit)
	{
		return false;
	}

	const int region = puzzle.region_at(row, col);

	const std::uint32_t region_bit = std::uint32_t{1} << region;

	if(used_regions & region_bit)
	{
		return false;
	}

	if(row > 0)
	{
		const int previous_col = cat_columns[static_cast<std::size_t>(row - 1)];

		if(previous_col >= 0)
		{
			int distance = std::abs(col - previous_col);
			if(distance <= 1)
			{
				return false;
			}
		}
	}
	
	return true;
}


void GameSolver::record_solution(
		const Puzzle& puzzle,
		const std::vector<int>& cat_columns,
		SolveResult& result)
{
	++result.solution_count;

	if(result.solution_count != 1)
	{
		return;
	}

	result.first_solution.clear();

	result.first_solution.reserve(static_cast<std::size_t>(puzzle.size));

	for(int row = 0; row < puzzle.size; ++row)
	{
		result.first_solution.push_back(
				{row,
				cat_columns[static_cast<std::size_t>(row)]});
	}
}


}





