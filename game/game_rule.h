#ifndef GAME_RULE_H
#define GAME_RULE_H

#include <string>
#include <vector>

#include "game_types.h"

namespace game
{

class GameRule
{
public:
    struct CheckResult
    {
        bool valid = false;
        std::string message;
    };

public:
    GameRule() = delete;

    static bool valid_size(int size) noexcept;

    /*
     * 检查 Puzzle 本身是否合法：
     * size 是否是 5~16
     * regions 是否正好 N*N
     * region id 是否是 0~N-1
     * N 个区域是否全部存在
     */
    static CheckResult check_puzzle(const Puzzle& puzzle);



	/*
     * 检查一组完整猫位置是否满足：每行一只\每列一只\8邻域无猫
     */
	static CheckResult check_cat_positions(int size, const std::vector<Position>& cats);



    /*
     * 检查一组完整猫位置是否满足：每行一只\每列一只\8邻域无猫\每区域一只
     */
    static CheckResult check_solution(const Puzzle& puzzle, const std::vector<Position>& cats);



};

}

#endif
