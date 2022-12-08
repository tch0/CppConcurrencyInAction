#include <iostream>
#include <iomanip>
#include <thread>
#include <future>
#include <mutex>
#include <chrono>
#include <atomic>
#include <vector>
#include <memory>
#include <functional>

using namespace std::chrono_literals;

// from P187.FineGrainedThreadSafeQueue.cpp
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

class ThreadPool
{
    std::atomic<bool> done;
    ThreadSafeQueue<std::function<void()>> workQueue;
    std::vector<std::jthread> threads;
    void workerThread()
    {
        while (!done)
        {
            if (auto spTask = workQueue.tryPop())
            {
                (*spTask)();
            }
            else
            {
                std::this_thread::yield();
            }
        }
    }
public:
    ThreadPool() : done(false)
    {
        const std::size_t threadCount = std::thread::hardware_concurrency();
        try
        {
            for (std::size_t i = 0; i < threadCount; ++i)
            {
                threads.push_back(std::jthread(&ThreadPool::workerThread, this));
            }
        }
        catch(...)
        {
            done = true;
            throw;
        }
    }
    ~ThreadPool()
    {
        done = true;
    }
    template<typename FunctionType>
    void submit(FunctionType f)
    {
        workQueue.push(std::function<void()>(f));
    }
};

std::mutex mcout; // mcout

int main(int argc, char const *argv[])
{
    std::atomic<int> counter = 0;
    auto f = [&counter]() {
        {
            std::lock_guard<std::mutex> lg(mcout);
            std::cout << "thread " << std::setw(2) << std::this_thread::get_id() << " : counter : "<< counter++ << std::endl;
        }
        std::this_thread::sleep_for(10ms);
    };
    // 100 tasks
    {
        ThreadPool pool;
        for (int i = 0; i < 100; ++i)
        {
            pool.submit(f);
        }
        std::this_thread::sleep_for(1000ms);
    }
    return 0;
}
