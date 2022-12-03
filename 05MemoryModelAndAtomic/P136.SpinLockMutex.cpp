#include <iostream>
#include <atomic>
#include <mutex>
#include <thread>
#include <chrono>
#include <vector>

using namespace std::chrono_literals;

class SpinLockMutex
{
    std::atomic_flag flag;
public:
    SpinLockMutex() : flag{ ATOMIC_FLAG_INIT } {};
    void lock()
    {
        while (flag.test_and_set(std::memory_order_acquire))
        {
        }
    }
    void unlock()
    {
        flag.clear(std::memory_order_release);
    }
};

SpinLockMutex mcout; // for cout
void f()
{
    for (int i = 0; i < 5; i++)
    {
        std::this_thread::sleep_for(10ms);
        std::lock_guard lg(mcout);
        std::cout << "thread " << std::this_thread::get_id() << " : " << i << std::endl;
    }
}

int main(int argc, char const *argv[])
{
    std::vector<std::jthread> vec;
    for (int i = 0; i < 10; i++)
    {
        vec.emplace_back(f);
    }
    return 0;
}
