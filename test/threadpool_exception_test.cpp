#include <atomic>
#include <cassert>
#include <iostream>
#include <stdexcept>

#include "../threadpool/threadpool.h"

struct TestTask
{
    bool should_throw = false;
    std::atomic<int>* completed = nullptr;

    void process()
    {
        if(should_throw)
        {
            throw std::runtime_error("test exception");
        }

        completed->fetch_add(1, std::memory_order_relaxed);
    }
};

int main()
{
    std::atomic<int> completed{0};

    TestTask bad_task;
    bad_task.should_throw = true;
    bad_task.completed = &completed;

    TestTask good_task;
    good_task.should_throw = false;
    good_task.completed = &completed;

    {
        threadpool<TestTask> pool(1, 10);

        assert(pool.append(&bad_task));
        assert(pool.append(&good_task));
    }

    assert(completed.load(std::memory_order_relaxed) == 1);

    std::cout << "Threadpool exception test passed\n";

    return 0;
}
