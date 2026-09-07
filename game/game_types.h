#ifndef GAME_TYPES_H
#define GAME_TYPES_H

#include <cstddef>
#include <vector>

namespace game
{

struct Position
{
    int row = -1;
    int col = -1;

    bool operator==(const Position& other) const noexcept
    {
        return row == other.row &&
               col == other.col;
    }
};

enum class CellState
{
    Unknown,
    Cat,
    Excluded
};


/*
 * Puzzle：
 * 表示玩家能够看到的。
 * 不包含 solution。
 */
struct Puzzle
{
    int size = 0;

    //regions 使用一维数组保存
    std::vector<int> regions;

    std::size_t index(int row, int col) const noexcept
    {
        return
            static_cast<std::size_t>(row) *
            static_cast<std::size_t>(size) +
            static_cast<std::size_t>(col);
    }

    int region_at(int row, int col) const noexcept
    {
        return regions[index(row, col)];
    }
};


/*
 * Level： 服务器内部完整关卡。
 *
 * puzzle：可以发送给客户端。
 *
 * solution：只能留在服务器。
 */
struct Level
{
    Puzzle puzzle;

    /*
     * size 个位置。
     * 每一个 Position 都是一只猫。
     */
    std::vector<Position> solution;
};

}

#endif
