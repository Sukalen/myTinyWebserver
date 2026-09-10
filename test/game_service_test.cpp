#include <cassert>
#include <iostream>
#include <chrono>
#include <thread>

#include "../service/game_service.h"

int main()
{
    game::GameService service;

    /*
     * docs/api.md：
     * V1 不支持 4阶。
     */
    auto invalid_size = service.start_game(4);

    assert(
        invalid_size.code ==
        game::GameService::Code::UnsupportedBoardSize);


    /*
     * 创建5阶游戏。
     */
    auto start = service.start_game(5);

    assert(start.success());
    assert(!start.session_id.empty());
    assert(start.state.puzzle.size == 5);
    assert(start.state.puzzle.regions.size() == 25);
    assert(start.state.cells.size() == 25);
    assert(start.state.hint_used == 0);
    assert(!start.state.completed);

    const std::string session_id = start.session_id;


    /*
     * 查询状态。
     */
    auto state = service.get_state(session_id);

    assert(state.success());
    assert(state.session_id == session_id);


    /*
     * 非法坐标。
     */
    auto invalid_position = service.action(
        session_id,
        -1,
        0,
        game::CellState::Cat);

    assert(
        invalid_position.code ==
        game::GameService::Code::InvalidPosition);


    /*
     * 合法玩家操作。
     */
    auto action = service.action(
        session_id,
        0,
        0,
        game::CellState::Excluded);

    assert(action.success());

    assert(
        action.state.cells[
            action.state.puzzle.index(0, 0)] ==
        game::CellState::Excluded);


    /*
     * 测试提示。
     */
    auto hint = service.hint(session_id);

    assert(hint.success());
    assert(hint.state.hint_used == 1);

    const std::size_t hint_index =
        hint.state.puzzle.index(
            hint.position.row,
            hint.position.col);

    assert(
        hint.state.cells[hint_index] ==
        game::CellState::Cat);


    /*
     * 不存在的 Session。
     */
    auto missing = service.get_state("not-exist");

    assert(
        missing.code ==
        game::GameService::Code::SessionNotFound);


    assert(service.session_count() == 1);


	{
    game::GameService timeout_service(std::chrono::seconds(1));

    auto start = timeout_service.start_game(5);

    assert(start.success());
    assert(timeout_service.session_count() == 1);

    std::this_thread::sleep_for(std::chrono::milliseconds(1100));

    const std::size_t removed = timeout_service.cleanup_expired_sessions();

    assert(removed == 1);
    assert(timeout_service.session_count() == 0);

    auto state = timeout_service.get_state(start.session_id);

    assert(state.code == game::GameService::Code::SessionNotFound);
	}

	{
    game::GameService timeout_service(std::chrono::seconds(1));

    auto start = timeout_service.start_game(5);

    assert(start.success());

    std::this_thread::sleep_for(std::chrono::milliseconds(600));

    auto state = timeout_service.get_state(start.session_id);

    assert(state.success());

    std::this_thread::sleep_for(std::chrono::milliseconds(600));

    const std::size_t removed = timeout_service.cleanup_expired_sessions();

    assert(removed == 0);
    assert(timeout_service.session_count() == 1);
	}

    std::cout << "GameService test passed\n";

    return 0;
}
