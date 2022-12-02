#include <iostream>
#include <thread>
#include <mutex>
#include <barrier>
#include <vector>

std::mutex mcout;
std::barrier<> br(10);
void f(bool discardAtLastStage = true)
{
    {
        std::lock_guard lg(mcout);
        std::cout << "first step of thread " << std::this_thread::get_id() << std::endl;
    }
    br.arrive_and_wait();
    {
        std::lock_guard lg(mcout);
        std::cout << "second step of thread " << std::this_thread::get_id() << std::endl;
    }
    if (discardAtLastStage)
    {
        br.arrive_and_drop(); // count down and drop this thread from the set
        return;
    }
    br.wait(br.arrive()); // equal to br.arrive_and_wait()
    {
        std::lock_guard lg(mcout);
        std::cout << "last stage of thread " << std::this_thread::get_id() << std::endl;
    }
}

int main(int argc, char const *argv[])
{
    std::vector<std::jthread> vec;
    for (int i = 0; i < 5; ++i)
    {
        vec.emplace_back(f, true);
        vec.emplace_back(f, false);
    }
    return 0;
}
