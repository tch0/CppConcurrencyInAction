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

using namespace std::chrono_literals;

// from 03SharedData/P49.ThreadSafeStack.cpp
template<typename T>
class ThreadSafeStack
{
private:
    std::stack<T> data;
    mutable std::mutex m;
public:
    ThreadSafeStack() {}
    ThreadSafeStack(const ThreadSafeStack& other)
    {
        std::lock_guard lock(other.m);
        data = other.data;
    }
    ThreadSafeStack& operator=(const ThreadSafeStack& other) = delete;
    void push(T new_value)
    {
        std::lock_guard lock(m);
        data.push(std::move(new_value));
    }
    std::shared_ptr<T> pop()
    {
        std::lock_guard lock(m);
        if (data.empty())
        {
            return std::shared_ptr<T>();
        }
        const std::shared_ptr<T> res(std::make_shared<T>(std::move(data.top())));
        data.pop();
        return res;
    }
    bool empty() const
    {
        std::lock_guard lock(m);
        return data.empty();
    }
};

template<typename T>
struct QuickSorter
{
    struct ChunkToSort
    {
        std::list<T> data;
        std::promise<std::list<T>> promise;
    };
    ThreadSafeStack<ChunkToSort> chunks;
    std::vector<std::jthread> threads;
    const std::size_t maxThreadCount;
    std::atomic<bool> endOfData;
    QuickSorter()
        : maxThreadCount(std::thread::hardware_concurrency() - 1)
        , endOfData(false)
    {
    }
    ~QuickSorter()
    {
        endOfData = true;
    }
    void trySortChunk()
    {
        auto chunk = chunks.pop();
        if (chunk)
        {
            sortChunk(chunk);
        }
    }
    std::list<T> doSort(std::list<T>& chunkData)
    {
        if (chunkData.empty())
        {
            return chunkData;
        }
        std::list<T> result;
        result.splice(result.begin(), chunkData, chunkData.begin()); // only transfer the first element as pivot.
        const T& pivot = *result.begin();
        auto dividePoint = std::partition(chunkData.begin(), chunkData.end(), [&](const T& val) { return val < pivot; });
        ChunkToSort newLowerChunk;
        newLowerChunk.data.splice(newLowerChunk.data.end(), chunkData, chunkData.begin(), dividePoint);
        std::future<std::list<T>> newLower = newLowerChunk.promise.get_future();
        chunks.push(std::move(newLowerChunk));
        if (threads.size() < maxThreadCount)
        {
            threads.emplace_back(std::jthread(&QuickSorter<T>::sortThread, this));
        }
        std::list<T> newHigher(doSort(chunkData));
        result.splice(result.end(), newHigher);
        while (newLower.wait_for(0ms) != std::future_status::ready)
        {
            trySortChunk();
        }
        result.splice(result.begin(), newLower.get());
        return result;
    }
    void sortChunk(const std::shared_ptr<ChunkToSort>& chunk)
    {
        chunk->promise.set_value(doSort(chunk->data));
    }
    void sortThread()
    {
        while (!endOfData)
        {
            trySortChunk();
            std::this_thread::yield();
        }
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
