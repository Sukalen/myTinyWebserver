#include "game_rule.h"

#include<cstdlib>
#include<vector>

namespace game
{

bool GameRule::valid_size(int size) noexcept
{
	return size >=5 && size <=16;
}


GameRule::CheckResult
GameRule::check_puzzle(const Puzzle& puzzle)
{
	if(!valid_size(puzzle.size))
	{
		return {false, "board size must be between 5 and 16"};
	}

	const int size = puzzle.size;

	const std::size_t expected_cells = static_cast<std::size_t>(size)*static_cast<std::size_t>(size);

	if(puzzle.regions.size() != expected_cells)
	{
		return {false, "region map size does not match board size"};
	}

	std::vector<int> region_count(static_cast<std::size_t>(size), 0);

	for(int region : puzzle.regions)
	{
		if( region < 0 || region >=size)
		{
			return {false, "invalid region id"};
		}

		++region_count[static_cast<std::size_t>(region)];
	}

	for(int count:region_count)
	{
		if( 0 == count)
		{
			return {false, "every region must contain at least one cell"};
		}
	}

	//检查颜色区域连通性
	for(int region = 0; region < size; ++region)
	{
		Position start{-1, -1};

		for(int row = 0; row < size && -1 == start.row; ++row)
		{
			for(int col = 0; col < size; ++col)
			{
				if(puzzle.region_at(row, col) == region)
				{
					start = {row, col};
					break;
				}
			}
		}

		if( -1 == start.row)
		{
			return {false,"region does not exist"};
		}

		std::vector<bool> visited(expected_cells, false);
		std::vector<Position> stack;
		
		stack.push_back(start);

		int connected_count = 0;

		while(!stack.empty())
		{
			Position current = stack.back();
			stack.pop_back();

			const std::size_t current_index = puzzle.index(current.row, current.col);

			if(visited[current_index])
			{
				continue;
			}

			visited[current_index] = true;

			if(puzzle.region_at(current.row, current.col) != region)
			{
				continue;
			}

			++connected_count;

			static const int directions[4][2] =
        	{
            	{-1, 0},
            	{1, 0},
            	{0, -1},
            	{0, 1}
        	};

			for(const auto& direction:directions)
			{
				const int next_row = current.row + direction[0];
				const int next_col = current.col + direction[1];

				if(next_row < 0 || next_row >= size ||
						next_col < 0 || next_col >= size)
				{
					continue;
				}

				const std::size_t next_index = puzzle.index(next_row, next_col);
				if(visited[next_index])
				{
					continue;
				}
				if(puzzle.region_at(next_row, next_col) == region)
				{
					stack.push_back({next_row, next_col});
				}
			}
		}

		if(connected_count != region_count[static_cast<std::size_t>(region)])
		{
			return {false, "each region must be connected"};
		}
	}

	return {true, "valid puzzle"};
}

GameRule::CheckResult
GameRule::check_cat_positions(int size, const std::vector<Position>& cats)
{
	if(!valid_size(size))
	{
		return {false, "board size must be between 5 and 16"};
	}

	if(cats.size() != static_cast<std::size_t>(size))
	{
		return {false, "solution must contain exactly N cats"};
	}

	std::vector<int> row_count(static_cast<std::size_t>(size), 0);
	std::vector<int> col_count(static_cast<std::size_t>(size), 0);


	//检查是否合法，并统计行列
	for(const Position& cat:cats)
	{
		if(cat.row < 0 || cat.row >= size ||
				cat.col < 0 || cat.col >= size)
		{
			return {false, "cat position is outsize board"};
		}

		++row_count[static_cast<std::size_t>(cat.row)];
		++col_count[static_cast<std::size_t>(cat.col)];

	}

	//检查每一行必须正好一只
	for(int count:row_count)
	{
		if(count!=1)
		{
			return {false, "each row must contain exactly one cat"};
		}

	}


	//检查每一列必须正好一只
	for(int count:col_count)
	{
		if(count != 1)
		{
			return {false, "each column must contain exactly one cat"};
		}
	}


	//检查每一只猫的8邻域，暴力算法
	for(std::size_t i = 0; i < cats.size(); ++i)
	{
		for(std::size_t j = i+1; j < cats.size(); ++j)
		{
			const int row_distance = std::abs(cats[i].row - cats[j].row);
			const int col_distance = std::abs(cats[i].col - cats[j].col);

			if(row_distance <= 1 && col_distance <= 1)
			{
				return {false, "cats must not touch each other"};
			}
		}
	}

	return {true,"valid cat positions"};

}

GameRule::CheckResult
GameRule::check_solution(const Puzzle& puzzle, const std::vector<Position>& cats)
{
	CheckResult puzzle_result = check_puzzle(puzzle);

	if(!puzzle_result.valid)
	{
		return puzzle_result;
	}

	CheckResult cat_result = check_cat_positions(puzzle.size, cats);
	if(!cat_result.valid)
	{
		return cat_result;
	}


	const int size = puzzle.size;

	std::vector<int> region_count(static_cast<std::size_t>(size), 0);

    for(const Position& cat : cats)
    {
        const int region = puzzle.region_at(cat.row, cat.col);

        ++region_count[static_cast<std::size_t>(region)];
    }

	//检查每一个颜色区域必须正好一只
	for(int count:region_count)
	{
		if(count != 1)
		{
			return {false, "each region must contain exactly one cat"};
		}
	}

	return {true, "valid solution"};
}

}


