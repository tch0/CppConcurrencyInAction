#include <iostream>
#include <mutex>
#include <future>
#include <thread>
#include <latch>
#include <vector>

std::latch la(10);
std::mutex mcout; // for cout

void f1()
{
    la.count_down();
    la.wait();
    {
        std::lock_guard lg(mcout);
        std::cout << "end of thread " << std::this_thread::get_id() << std::endl;
    }
}

int main(int argc, char const *argv[])
{
    std::vector<std::jthread> vec;
    for (int i = 0; i < 10; ++i)
    {
        vec.emplace_back(f1);
    }
    return 0;
}
