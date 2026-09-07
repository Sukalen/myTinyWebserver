#ifndef GAME_SOLVER_H
#define GAME_SOLVER_H

#include <cstdint>
#include <string>
#include <vector>

#include "game_types.h"

namespace game
{

class GameSolver
{
public:
	struct SolveResult
	{
		//Puzzle本身是否合法
		bool valid_puzzle = false;

		//实际统计到的解数量
		int solution_count = 0;

		//保存找到的第一个解
		std::vector<Position> first_solution;

		std::string message;

		bool unique() const noexcept
		{
			return valid_puzzle && 1==solution_count;
		}
	};

public:
	GameSolver() = delete;

	//limit为2表示默认找到第二个解就立即停止
	static SolveResult solve(const Puzzle& puzzle, int limit = 2);

private:
	static void dfs(
			const Puzzle& puzzle,
        	int row,
        	int limit,
        	std::uint32_t used_columns,
        	std::uint32_t used_regions,
        	std::vector<int>& cat_columns,
        	SolveResult& result);

    static bool can_place(
        	const Puzzle& puzzle,
        	int row,
        	int col,
        	std::uint32_t used_columns,
        	std::uint32_t used_regions,
        	const std::vector<int>& cat_columns);

	static void record_solution(
			const Puzzle& puzzle,
			const std::vector<int>& cat_columns,
			SolveResult& result);
};

}

#endif

