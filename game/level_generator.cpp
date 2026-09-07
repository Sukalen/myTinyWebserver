#include "level_generator.h"
#include "game_rule.h"
#include "game_solver.h"


#include <algorithm>
#include <numeric>
#include <stdexcept>


namespace
{

std::size_t cell_index(
    int row,
    int col,
    int size)
{
    return static_cast<std::size_t>(row) *
        static_cast<std::size_t>(size) +
        static_cast<std::size_t>(col);
}


std::vector<int> collect_neighbor_regions(
    int row,
    int col,
    int size,
    const std::vector<int>& regions)
{
    std::vector<int> neighbor_regions;

    static const int directions[4][2] =
    {
        {-1, 0},
        {1, 0},
        {0, -1},
        {0, 1}
    };

    for(const auto& direction : directions)
    {
        const int next_row = row + direction[0];

        const int next_col = col + direction[1];

        if(next_row < 0 || next_row >= size ||
           next_col < 0 || next_col >= size)
        {
            continue;
        }

        const int region =
            regions[cell_index(next_row, next_col, size)];

        //-1表示尚未分配区域。
         
        if(region < 0)
        {
            continue;
        }

        
        // 一个格子可能同时接触同一区域的多个格子
        if(std::find(
                neighbor_regions.begin(),
                neighbor_regions.end(),
                region) ==
           neighbor_regions.end())
        {
            neighbor_regions.push_back(region);
        }
    }

    return neighbor_regions;
}

}

namespace game
{


LevelGenerator::LevelGenerator()
	:m_rng(std::random_device{}())
{}

LevelGenerator::LevelGenerator(std::uint32_t seed)
	:m_rng(seed)
{}


std::vector<Position> LevelGenerator::generate_cat_positions(int size)
{
	if(!GameRule::valid_size(size))
	{
		throw std::invalid_argument("board size must be between 5 and 16");
	}

	std::vector<int> cat_columns(static_cast<std::size_t>(size), -1);

	const bool success = generate_cat_columns(
											0,
											size,
											0,
											cat_columns);

	if(!success)
	{
		throw std::runtime_error("failed to generate cat positions");
	}

	std::vector<Position> cats;
	
	cats.reserve(static_cast<std::size_t>(size));

	for(int row = 0; row < size; ++row)
	{
		cats.push_back(
				{row,
				cat_columns[static_cast<std::size_t>(row)]});
	}

	return cats;
}

bool LevelGenerator::generate_cat_columns(
		int row,
		int size,
		std::uint32_t used_columns,
		std::vector<int>& cat_columns)
{
	if(row == size)
	{
		return true;
	}

	std::vector<int> candidates(static_cast<std::size_t>(size));

	std::iota(candidates.begin(), candidates.end(), 0);

	//随机打乱尝试顺序
	std::shuffle(candidates.begin(), candidates.end(), m_rng);

	for(int col:candidates)
	{
		const std::uint32_t bit = std::uint32_t{1} << col;

		if(used_columns & bit)
		{
			continue;
		}

		if(row>0)
		{
			const int previous_col = cat_columns[static_cast<std::size_t>(row-1)];
			int distance = std::abs(col - previous_col);

			if(distance <=1)
			{
				continue;
			}
		}

		cat_columns[static_cast<std::size_t>(row)] = col;

		if(generate_cat_columns(
					row+1,
					size,
					used_columns | bit,
					cat_columns))
		{
			return true;
		}

		cat_columns[static_cast<std::size_t>(row)] = -1;
	}

	return false;
}


std::vector<int> LevelGenerator::generate_regions(
		int size, 
		const std::vector<Position>& cats)
{
    GameRule::CheckResult cat_result =
        GameRule::check_cat_positions(size, cats);

    if(!cat_result.valid)
    {
        throw std::invalid_argument(cat_result.message);
    }

    const std::size_t total_cells = static_cast<std::size_t>(size) *static_cast<std::size_t>(size);

    
    //-1 表示格子暂时没有区域
    std::vector<int> regions(total_cells, -1);

	//初始化
	for(int region = 0; region < size; ++region)
	{
		const Position& cat = cats[static_cast<std::size_t>(region)];

		regions[cell_index(cat.row, cat.col, size)] = region;
	}

	int remaining = size*size - size;

	//填充
	while(remaining > 0)
	{
		std::vector<Position> candidates;

		for(int row = 0; row < size; ++row)
		{
			for(int col = 0;col<size;++col)
			{
				const std::size_t index = cell_index(row,col,size);

				if(regions[index] >= 0)
                {
                    continue;
                }

                std::vector<int> neighbor_regions =
                        		collect_neighbor_regions(
                            		row,
                            		col,
                            		size,
                            		regions);
				if(!neighbor_regions.empty())
				{
					candidates.push_back({row,col});
				}
			}
		}

		if(candidates.empty())
        {
            throw std::runtime_error("region growth got stuck");
        }

		std::uniform_int_distribution<std::size_t> cell_distribution(
   			0,
    		candidates.size() - 1);

		const Position chosen = candidates[cell_distribution(m_rng)];

		std::vector<int> neighbor_regions =
                collect_neighbor_regions(
                    chosen.row,
                    chosen.col,
                    size,
                    regions);

		std::uniform_int_distribution<std::size_t> region_distribution(
                0,
                neighbor_regions.size() - 1);

        const int chosen_region = neighbor_regions[region_distribution(m_rng)];

		regions[cell_index(chosen.row,chosen.col,size)] = chosen_region;

		--remaining;
	}
	return regions;

}


Level LevelGenerator::generate(int size)
{
	if(!GameRule::valid_size(size))
	{
		throw std::invalid_argument("board size must be between 5 and 16");
	}

	const int max_region_attempts = 100;

	const int max_cat_attempts = 30;

	for(int cat_attempt = 0;cat_attempt<max_cat_attempts;++cat_attempt)
	{
		std::vector<Position> cats = generate_cat_positions(size);

		GameRule::CheckResult cat_result =GameRule::check_cat_positions(size, cats);

        if(!cat_result.valid)
        {
            continue;
        }

		for(int region_attempt = 0;region_attempt < max_region_attempts;++region_attempt)
		{
			Puzzle puzzle;
			
			puzzle.size = size;
			puzzle.regions = generate_regions(size, cats);

			GameRule::CheckResult solution_result = GameRule::check_solution(puzzle, cats);
            if(!solution_result.valid)
            {
                continue;
            }

			GameSolver::SolveResult solve_result = GameSolver::solve(puzzle,2);
			if(!solve_result.valid_puzzle)
			{
				continue;
			}
			if(!solve_result.unique())
			{
				continue;
			}

			Level level;
			level.puzzle = std::move(puzzle);
			level.solution = std::move(cats);
			return level;
		}
	}
	
	throw std::runtime_error("failed to generate unique level");
}


}

