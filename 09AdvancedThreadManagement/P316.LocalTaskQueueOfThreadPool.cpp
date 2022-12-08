#include <iostream>
#include <thread>
#include <future>
#include <list>
#include <vector>
#include <stack>
#include <memory>
#include <algorithm>
#include <numeric>
#include <chrono>
#include <random>
#include <queue>

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

std::mutex mcout; // for cout

class ThreadPool
{
    using LocalQueueType = std::queue<FunctionWrapper>;
    std::atomic<bool> done;
    ThreadSafeQueue<FunctionWrapper> workQueue;
    std::vector<std::jthread> threads;
    inline static thread_local std::unique_ptr<LocalQueueType> localWorkQueue;
    void workerThread()
    {
        localWorkQueue.reset(new LocalQueueType);
        while (!done)
        {
            runPendingTask();
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
        if (localWorkQueue)
        {
            localWorkQueue->push(std::move(task));
        }
        else
        {
            workQueue.push(std::move(task));
        }
        return res;
    }
    void runPendingTask()
    {
        if (localWorkQueue && !localWorkQueue->empty())
        {
            {
                std::lock_guard lk(mcout);
                std::cout << "Run task in local work queue in thread " << std::this_thread::get_id() << std::endl;
            }
            FunctionWrapper task = std::move(localWorkQueue->front());
            localWorkQueue->pop();
            task();
        }
        else if (auto spTask = workQueue.tryPop())
        {
            {
                std::lock_guard lk(mcout);
                std::cout << "Run task in pool work queue in thread " << std::this_thread::get_id() << std::endl;
            }
            (*spTask)();
        }
        else
        {
            std::this_thread::yield();
        }
    }
};

// for test
template<typename T>
struct QuickSorter
{
    ThreadPool pool;
    std::list<T> doSort(std::list<T>& chunkData)
    {
        if (chunkData.empty())
        {
            return chunkData;
        }
        std::list<T> result;
        result.splice(result.begin(), chunkData, chunkData.begin());
        const T& pivot = *result.begin();
        auto dividePoint = std::partition(chunkData.begin(), chunkData.end(),
            [&pivot](const T& value) { return value < pivot; });
        std::list<T> newLowerChunk;
        newLowerChunk.splice(newLowerChunk.begin(), chunkData, chunkData.begin(), dividePoint);
        std::future<std::list<T>> newLower = pool.submit(
            [this, &newLowerChunk]() -> decltype(auto) { 
                return this->doSort(newLowerChunk);
            }
        );
        std::list<T> newHigher(doSort(chunkData));
        result.splice(result.end(), newHigher); // higher part
        while (newLower.wait_for(0ms) == std::future_status::timeout) // get tasks and run when waiting for lower part done.
        {
            pool.runPendingTask();
        }
        result.splice(result.begin(), newLower.get()); // lower part
        return result;
    }
};

template<typename T>
std::list<T> parallelQuickSort(std::list<T> input)
{
    if (input.empty())
    {
        return input;
    }
    QuickSorter<T> s;
    return s.doSort(input);
}

int main(int argc, char const *argv[])
{
    std::vector<int> vec(100, 0);
    std::iota(vec.begin(), vec.end(), 0);
    std::shuffle(vec.begin(), vec.end(), std::mt19937());
    std::list<int> ltest(vec.begin(), vec.end());
    auto res = parallelQuickSort(ltest);
    for (const auto& v : res)
    {
        std::cout << v << ", ";
    }
    std::cout << std::endl;
    std::cout << std::boolalpha << std::is_sorted(res.begin(), res.end()) << std::endl;
    return 0;
}
