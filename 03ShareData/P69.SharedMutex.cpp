#include <iostream>
#include <iomanip>
#include <thread>
#include <mutex>
#include <shared_mutex>
#include <chrono>
#include <vector>
#include <atomic>

using namespace std::chrono_literals;

std::shared_mutex rwm;
std::mutex mcout;
std::vector<int> vec(5, 99); // data
std::atomic_flag end_of_write { ATOMIC_FLAG_INIT };

void printVector(const std::vector<int>& vec)
{
    std::lock_guard guard(mcout);
    std::cout << "thread " << std::setw(2) << std::this_thread::get_id() << " [";
    for (auto& v : vec)
    {
        std::cout << v << ", ";
    }
    std::cout << "]" << std::endl;
}

void read()
{
    while (!end_of_write.test())
    {
        std::this_thread::sleep_for(80ms);
        std::shared_lock lck(rwm);
        printVector(vec);
    }
}

void write()
{
    for (int i = 0; i < 10; ++i)
    {
        std::this_thread::sleep_for(50ms);
        std::lock_guard lck(rwm);
        vec.push_back(i);
    }
    end_of_write.test_and_set();
}

int main(int argc, char const *argv[])
{
    std::jthread w(write);
    std::vector<std::jthread> rthreads;
    for (int i = 0; i < 10; ++i)
    {
        rthreads.emplace_back(read);
    }
    return 0;
}
