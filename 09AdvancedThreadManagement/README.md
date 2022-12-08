<!-- START doctoc generated TOC please keep comment here to allow auto update -->
<!-- DON'T EDIT THIS SECTION, INSTEAD RE-RUN doctoc TO UPDATE -->
**Table of Contents**  *generated with [DocToc](https://github.com/thlorenz/doctoc)*

- [第九章：高级线程管理](#%E7%AC%AC%E4%B9%9D%E7%AB%A0%E9%AB%98%E7%BA%A7%E7%BA%BF%E7%A8%8B%E7%AE%A1%E7%90%86)
  - [线程池](#%E7%BA%BF%E7%A8%8B%E6%B1%A0)
  - [中断线程](#%E4%B8%AD%E6%96%AD%E7%BA%BF%E7%A8%8B)

<!-- END doctoc generated TOC please keep comment here to allow auto update -->

# 第九章：高级线程管理

## 线程池

最简易可行的线程池：
- 通过一个任务队列，提交任务后保存到队列中。
- 采用数目固定的工作线程（往往与`std::thread::hardward_concurrency()`返回值相等）：
    - 每个工作线程从任务队列中领取一个任务并执行，并随时判断是否结束，如此循环往复。
    - 如果没有领取到任务，那么就提示让出时间片给其他线程。
- 线程池析构时会提示工作线程任务结束，结束运行，并等待所有工作线程结束。 
- 其中任务队列采用前面实现的线程安全的队列。
- 见[P308.SimpleThreadPool.cpp](P308.SimpleThreadPool.cpp)。

等待提交给线程池的任务完成运行：
- 在显式使用线程执行一个任务时，我们等待的是线程执行完成，将结果通过`std::future`或者共享数据配合条件变量等机制通知等待的线程。
- 但在使用线程池时，等待的对象变成了一个个任务，而非工作线程本身。
- 为了能够实现等待任务完成的机制，需要手动在线程池中加入条件变量和`std::future`的使用。
- 使用`std::future`可以同时实现等待与传递结果，在任务提交时返回任务句柄（比如直接用`std::future`即可），用于等待任务完成。
- 通过`std::packaged_task`对任务进行包装得到对应的`std::future`，作为任务就可以做到。但是`std::packaged_task`是只移动类型，而`std::function<>`要求其所包含的函数对象可以拷贝构造。所以需要实现一个自己的函数包装器`FunctionWrapper`，同样实现为只移动类型。
- 修改后的线程池实现：[P311.WaitForThreadPool.cpp](P311.WaitForThreadPool.cpp)，通过`submit`返回提交任务的`std::future`对象即可等待任务完成。

等待其他任务完成的任务：
- 一个任务如果中中途需要等待其他任务完成，那么线程池就必然可能会陷入一中所有任务都在等待其他任务完成的情况。这时没有任何线程能够用来执行任务队列中剩余的任务，就死锁了。
- 而要解决这个问题，就需要在等待期间从线程池中提取任务出来执行，那么就需要对线程池做一个扩展，使之能够提取任务到当前线程中来执行：
```C++
void ThreadPool::runPendingTask()
{
    FunctionWrapper task;
    if (auto spTask = workQueue.tryPop())
    {
        (*spTask)();
    }
    else
    {
        std::this_thread::yield();
    }
}
```
- 为了能够在任务中调用到这个函数，通常需要对线程池做一个包装，将任务和线程吃包装到一起。
- 上一章中的并行快速排序其实就使用了这个技巧，这里同样以快速排序为例，只是改用通用的线程池实现（上一章其实实现了一个隐式的专用的线程池）：
```C++
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
```
- 完整实现见：[P315.ThreadPoolQuickSort.cpp](P315.ThreadPoolQuickSort.cpp)。

避免任务队列上的争夺：
- 线程池使用一个任务队列，提交任务时，可能出现多个线程争夺提交，提取任务出来执行时也可能出现多线程争夺。
- 当处理器数据增多时，这种争夺将会非常明显，即使使用无锁队列避免显式等待，仍然会产生缓存乒乓现象，导致性能损失与时间浪费。
- 一种解决方法是：为每个线程配备单独的任务队列。
    - 各个线程只在自己的队列上发布新任务，仅当自身的队列没有任务时，才从全局队列中领取任务。
    - 可以使用`thread_local`变量表示这个队列。
- 实现：[P316.LocalTaskQueueOfThreadPool.cpp](P316.LocalTaskQueueOfThreadPool.cpp)。
- 线程自身的队列可以用智能指针`std::unique_ptr`保存，并且在线程创建时才初始化，只有线程池中的线程会初始化，降低非池中线程的开销。
- 线程自身队列只会在当前线程内访问，不需要使用线程安全的队列，`std::queue`就已经足够。

任务窃取：
- 在上一个实现的基础上，可能出现当前任务队列已经处理完，并且全局队列也没有任务，但其他线程任务很多的情况。
- 这种情况下，如果可以从其他线程窃取任务到当前线程来执行，那么就能保证只有有任务所有线程都不会闲着，又能最大程度避免竞争。
- 这种实现需要将线程局部的队列改为可以共享的队列：
    - 这个共享队列数组由线程池进行管理，并在每个线程中使用一个`thread_local`的`static`变量保存当前线程对应的索引。
    - 现在可以并发访问了，每个队列需要使用线程安全的队列。
    - 预计中这种并发访问应该是比较少的情况，不会有很强的数据竞争，也可以用普通队列用包装一层使用互斥实现多线程访问（而不必使用资源消耗会比较多、锁粒度比较小的线程安全队列）。
    - 窃取任务是可以从尾部进行，以提高并发度。任务队列中相邻任务一般都有关系，可能会共用内存，从尾部窃取任务也能最大程度地减少缓存失效。
- 一个支持任务窃取的队列见：[P322.StealingTasksOfThreadPool.cpp中的`WorkStealingQueue`](P322.StealingTasksOfThreadPool.cpp#L164)。
```C++
class WorkStealingQueue
{
private:
    using DataType = FunctionWrapper;
    std::deque<DataType> theQueue;
    mutable std::mutex theMutex;
public:
    WorkStealingQueue() {}
    WorkStealingQueue(const WorkStealingQueue&) = delete;
    WorkStealingQueue& operator=(const WorkStealingQueue&) = delete;
    void push(DataType data)
    {
        std::lock_guard lk(theMutex);
        theQueue.push_front(std::move(data));
    }
    bool empty() const
    {
        std::lock_guard lk(theMutex);
        return theQueue.empty();
    }
    bool tryPop(DataType& res) // pop tasks from front
    {
        std::lock_guard lk(theMutex);
        if (theQueue.empty())
        {
            return false;
        }
        res = std::move(theQueue.front());
        theQueue.pop_front();
        return true;
    }
    bool trySteal(DataType& res) // steal tasks from back
    {
        std::lock_guard lk(theMutex);
        if (theQueue.empty())
        {
            return false;
        }
        res = std::move(theQueue.back());
        theQueue.pop_back();
        return true;
    }
};
```
- 支持任务窃取的完整线程池实现见：[P322.StealingTasksOfThreadPool.cpp](P322.StealingTasksOfThreadPool.cpp)。

可以改进的点：
- 实践中针对具体应用，可能存在很多其他的改进方法，需要在实践中总结。
- 可以动态调整线程池规模使得CPU的使用效率最佳，其中甚至包括处理线程发生阻塞的情形，如等待IO或者等待互斥解锁。
    - 例如如果线程池中处于等待的线程很多，而只有少量线程处于运行状态，可以动态新增一个线程到线程池中。
    - 当等待中线程解锁后，又动态降低线程池中线程数量以维持处于运行状态的线程数量处于最优状态。

## 中断线程

许多情况中：如果线程运行时间过长，我们可能想让其停下来。
- 原因可能有很多：
    - 该线程是线程池内的工作线程，现在线程池要销毁，对其中的任务不感兴趣了。
    - 用户命令取消了线程正在执行的任务。
    - 整个程序结束了。
- 无论什么原因，操作思路都相同：
    - 给要结束的线程发送停止信号，让线程受到信号后自己结束，以妥善处理其中的数据。
- 我们可以为每一个中断线程的原因设计代码，但是这样未免太小题大做了。
- 可以实现通用的中断方式，以用于任何适用的情况。
- C++11标准库尚未提供这种方式，C++20引入的`std::jthread`提供了，这里先尝试自行实现而不适用标准库接口。最后再来讨论`std::jthread`。


