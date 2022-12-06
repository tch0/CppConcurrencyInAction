#include <iostream>
#include <iomanip>
#include <mutex>
#include <future>
#include <thread>
#include <memory>
#include <chrono>
#include <vector>
#include <queue>

using namespace std::chrono_literals;

template<typename T>
class ThreadSafeQueue
{
private:
    mutable std::mutex mut;
    std::queue<std::shared_ptr<T>> data;
    std::condition_variable cond;
public:
    ThreadSafeQueue()
    {
    }
    ThreadSafeQueue(const ThreadSafeQueue& other) = delete;
    ThreadSafeQueue& operator=(const ThreadSafeQueue&) = delete;
    void push(T value)
    {
        std::shared_ptr<T> newValuePtr = std::make_shared<T>(std::move(value));
        std::lock_guard lg(mut);
        data.push(newValuePtr);
        cond.notify_one();
    }
    bool try_pop(T& value)
    {
        std::lock_guard lg(mut);
        if (data.empty())
        {
            return false;
        }
        value = std::move(*data.front());
        data.pop();
        return true;
    }
    std::shared_ptr<T> try_pop()
    {
        std::lock_guard lg(mut);
        if (data.empty())
        {
            return std::shared_ptr<T>(); // empty shared_ptr
        }
        std::shared_ptr<T> res = data.front();
        data.pop();
        return res;
    }
    void wait_and_pop(T& value)
    {
        std::unique_lock ul(mut);
        cond.wait(ul, [this] { return !data.empty(); });
        value = std::move(*data.front());
        data.pop();
    }
    std::shared_ptr<T> wait_and_pop()
    {
        std::unique_lock ul(mut);
        cond.wait(ul, [this] { return !data.empty(); });
        std::shared_ptr<T> res = data.front();
        data.pop();
        return res;
    }
    bool empty() const
    {
        std::lock_guard lg(mut);
        return data.empty();
    }
};

// application
ThreadSafeQueue<double> Q;
std::mutex mcout; // for cout
std::atomic_flag end_of_produce { ATOMIC_FLAG_INIT };
void producer()
{
    for (int i = 0; i < 100; ++i)
    {
        Q.push(i);
        {
            std::lock_guard lg(mcout);
            std::cout << "thread " << std::setw(2) << std::this_thread::get_id() << " : producer : " << std::setw(2) << i << " +" << std::endl;
        }
        std::this_thread::sleep_for(10ms);
    }
    end_of_produce.test_and_set();
}
void consumer()
{
    while (!end_of_produce.test())
    {
        if (auto sp = Q.try_pop())
        {
            std::lock_guard lg(mcout);
            std::cout << "thread " << std::setw(2) << std::this_thread::get_id() << " : comsumer : " << std::setw(2) << *sp << " -" << std::endl;
        }
        std::this_thread::sleep_for(20ms);
    }
}

int main(int argc, char const *argv[])
{
    std::jthread tp(producer);
    std::vector<std::jthread> consumers;
    for (int i = 0; i < 10; ++i)
    {
        consumers.emplace_back(consumer);
    }
    return 0;
}
