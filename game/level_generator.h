#ifndef LEVEL_GENERATOR_H
#define LEVEL_GENERATOR_H

#include <cstdint>
#include <random>
#include <vector>

#include "game_types.h"


namespace game
{

class LevelGenerator
{
public:
	LevelGenerator();
	explicit LevelGenerator(std::uint32_t seed);

	std::vector<Position> generate_cat_positions(int size);

	std::vector<int> generate_regions(int size, const std::vector<Position>& cats);

	Level generate(int size);

private:
	bool generate_cat_columns(
			int row,
			int size,
			std::uint32_t used_columns,
			std::vector<int>& cat_columns);

private:
	std::mt19937 m_rng;
};

}



#endif
