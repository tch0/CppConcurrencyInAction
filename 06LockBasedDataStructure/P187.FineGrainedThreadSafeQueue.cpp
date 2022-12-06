#include <iostream>
#include <iomanip>
#include <thread>
#include <mutex>
#include <future>
#include <condition_variable>
#include <vector>
#include <chrono>
#include <memory>
#include <cassert>
using namespace std::chrono_literals;

template<typename T>
class ThreadSafeQueue
{
private:
    struct node
    {
        std::shared_ptr<T> data;
        struct std::unique_ptr<node> next;
    };
    std::mutex headMutex;
    std::unique_ptr<node> head;
    std::mutex tailMutex;
    node* tail;
    std::condition_variable dataCond;
    node* getTail()
    {
        std::lock_guard tailLock(tailMutex);
        return tail;
    }
    std::unique_ptr<node> popHead()
    {
        std::unique_ptr<node> oldHead = std::move(head);
        head = std::move(oldHead->next);
        return oldHead;
    }
    std::unique_lock<std::mutex> waitForData()
    {
        std::unique_lock<std::mutex> headLock(headMutex);
        dataCond.wait(headLock, [&]() -> bool { return head.get() != getTail(); });
        return headLock;
    }
    std::unique_ptr<node> waitPopHead()
    {
        std::unique_lock<std::mutex> headLock(waitForData());
        return popHead();
    }
    std::unique_ptr<node> waitPopHead(T& value)
    {
        std::unique_lock<std::mutex> headLock(waitForData());
        value = std::move(*head->data); // if throw exception here, no data will be removed.
        return popHead();
    }
    std::unique_ptr<node> tryPopHead()
    {
        std::lock_guard headLock(headMutex);
        if (head.get() == getTail())
        {
            return std::unique_ptr<node>();
        }
        return popHead();
    }
    std::unique_ptr<node> tryPopHead(T& value)
    {
        std::lock_guard headLock(headMutex);
        if (head.get() == getTail())
        {
            return std::unique_ptr<node>();
        }
        value = std::move(*head->data); // if throw exception here, no data will be removed.
        return popHead();
    }
public:
    ThreadSafeQueue() : head(std::make_unique<node>()), tail(head.get()) {}
    ThreadSafeQueue(const ThreadSafeQueue& other) = delete;
    ThreadSafeQueue& operator=(const ThreadSafeQueue& other) = delete;
    std::shared_ptr<T> tryPop()
    {
        std::unique_ptr<node> oldHead = tryPopHead();
        return oldHead ? std::move(oldHead->data) : std::shared_ptr<T>();
    }
    bool tryPop(T& value)
    {
        std::unique_ptr<node> oldHead = tryPopHead(value);
        return bool(oldHead);
    }
    std::shared_ptr<T> waitAndPop()
    {
        std::unique_ptr<node> oldHead = waitPopHead();
        return oldHead->data;
    }
    void waitAndPop(T& value)
    {
        std::unique_ptr<node> oldHead = waitPopHead(value);
    }
    void push(T value)
    {
        std::shared_ptr<T> newData = std::make_shared<T>(std::move(value));
        std::unique_ptr<node> p = std::make_unique<node>();
        node* newTail = p.get();
        {
            std::lock_guard tailLock(tailMutex); // minimize the critical section
            tail->data = std::move(newData);
            tail->next = std::move(p);
            tail = newTail;
        }
        dataCond.notify_one();
    }
    bool empty() const
    {
        std::lock_guard headLock(headMutex);
        return head.get() == getTail();
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

void consumer0()
{
    while (!end_of_produce.test())
    {
        if (auto sp = Q.tryPop())
        {
            std::lock_guard lg(mcout);
            std::cout << "thread " << std::setw(2) << std::this_thread::get_id() << " : consumer : " << std::setw(2) << *sp << " -" << std::endl;
        }
        std::this_thread::sleep_for(50ms);
    }
}

void consumer1()
{
    while (!end_of_produce.test())
    {
        double value {};
        if (Q.tryPop(value))
        {
            std::lock_guard lg(mcout);
            std::cout << "thread " << std::setw(2) << std::this_thread::get_id() << " : consumer : " << std::setw(2) << value << " -" << std::endl;
        }
        std::this_thread::sleep_for(50ms);
    }
}

void consumer2()
{
    while (!end_of_produce.test())
    {
        if (auto sp = Q.waitAndPop())
        {
            std::lock_guard lg(mcout);
            std::cout << "thread " << std::setw(2) << std::this_thread::get_id() << " : consumer : " << std::setw(2) << *sp << " -" << std::endl;
        }
        std::this_thread::sleep_for(20ms);
    }
}

void consumer3()
{
    while (!end_of_produce.test())
    {
        double value {};
        Q.waitAndPop(value);
        std::lock_guard lg(mcout);
        std::cout << "thread " << std::setw(2) << std::this_thread::get_id() << " : consumer : " << std::setw(2) << value << " -" << std::endl;
        std::this_thread::sleep_for(20ms);
    }
}

int main(int argc, char const *argv[])
{
    {
        std::jthread tp(producer);
        std::vector<std::jthread> consumers;
        for (int i = 0; i < 5; ++i)
        {
            consumers.emplace_back(consumer0);
            consumers.emplace_back(consumer1);
        }
    }
    std::cout << std::endl;
    end_of_produce.clear();
    {
        std::jthread tp(producer);
        std::vector<std::jthread> consumers;
        for (int i = 0; i < 5; ++i)
        {
            std::jthread(consumer2).detach();
            std::jthread(consumer3).detach();
        }
    }
    return 0;
}
