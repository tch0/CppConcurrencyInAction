#include <iostream>
#include <sstream>
#include <iomanip>
#include <thread>
#include <future>
#include <mutex>
#include <chrono>
#include <atomic>
#include <vector>
#include <memory>
#include <functional>
#include <type_traits>

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

// to replace std::function as task type of thread pool, move-only type
class FunctionWrapper
{
    struct ImplBase
    {
        virtual void call() = 0;
        virtual ~ImplBase() {}
    };
    std::unique_ptr<ImplBase> impl;
    template<typename F>
    struct ImplType : public ImplBase
    {
        F f;
        ImplType(F&& _f) : f(std::move(_f)) {}
        void call() { f(); }
    };
public:
    FunctionWrapper() = default;
    template<typename F>
    FunctionWrapper(F&& f) : impl(new ImplType<F>(std::move(f))) {}
    
    FunctionWrapper(const FunctionWrapper&) = delete;
    FunctionWrapper(FunctionWrapper&& other) : impl(std::move(other.impl)) {}
    
    FunctionWrapper& operator=(FunctionWrapper&& other)
    {
        impl = std::move(other.impl);
        return *this;
    }
    FunctionWrapper& operator=(const FunctionWrapper&) = delete;

    void operator()()
    {
        if (impl)
        {
            impl->call();
        }
    }
};

class ThreadPool
{
    std::atomic<bool> done;
    ThreadSafeQueue<FunctionWrapper> workQueue;
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
    std::future<typename std::invoke_result_t<FunctionType>> submit(FunctionType f)
    {
        using ResultType = typename std::invoke_result_t<FunctionType>;
        std::packaged_task<ResultType()> task(std::move(f));
        std::future<ResultType> res(task.get_future());
        workQueue.push(std::move(task));
        return res;
    }
};

std::mutex mcout; // for cout

int main(int argc, char const *argv[])
{
    std::atomic<int> counter = 0;
    auto f = [&counter]() -> int {
        int res;
        {
            std::lock_guard<std::mutex> lg(mcout);
            std::cout << "thread " << std::setw(2) << std::this_thread::get_id() << " : counter : "<< (res = counter++) << std::endl;
        }
        std::this_thread::sleep_for(10ms);
        return res;
    };
    // 100 tasks
    std::vector<std::future<int>> futVec;
    {
        ThreadPool pool;
        for (int i = 0; i < 100; ++i)
        {
            futVec.push_back(pool.submit(f));
        }
        std::ostringstream oss;
        for (std::size_t i = 0; i < futVec.size(); ++i)
        {
            oss << futVec[i].get() << ", ";
        }
        std::cout << oss.str() << std::endl;
    }
    return 0;
}
