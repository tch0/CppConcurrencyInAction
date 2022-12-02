<!-- START doctoc generated TOC please keep comment here to allow auto update -->
<!-- DON'T EDIT THIS SECTION, INSTEAD RE-RUN doctoc TO UPDATE -->
**Table of Contents**  *generated with [DocToc](https://github.com/thlorenz/doctoc)*

- [第四章：并发操作的同步](#%E7%AC%AC%E5%9B%9B%E7%AB%A0%E5%B9%B6%E5%8F%91%E6%93%8D%E4%BD%9C%E7%9A%84%E5%90%8C%E6%AD%A5)
  - [等待事件或其他条件](#%E7%AD%89%E5%BE%85%E4%BA%8B%E4%BB%B6%E6%88%96%E5%85%B6%E4%BB%96%E6%9D%A1%E4%BB%B6)
  - [使用future等待一次性事件发生](#%E4%BD%BF%E7%94%A8future%E7%AD%89%E5%BE%85%E4%B8%80%E6%AC%A1%E6%80%A7%E4%BA%8B%E4%BB%B6%E5%8F%91%E7%94%9F)
  - [限时等待](#%E9%99%90%E6%97%B6%E7%AD%89%E5%BE%85)
  - [运用同步操作简化代码](#%E8%BF%90%E7%94%A8%E5%90%8C%E6%AD%A5%E6%93%8D%E4%BD%9C%E7%AE%80%E5%8C%96%E4%BB%A3%E7%A0%81)

<!-- END doctoc generated TOC please keep comment here to allow auto update -->

# 第四章：并发操作的同步

## 等待事件或其他条件

实现线程甲等待线程乙完成某事件的手段：
- 共享数据内部维护受互斥保护的标志，线程乙完成任务后设置标志成立，线程甲不段检测标志。浪费线程甲处理时间，需要不断检测标志。
- 让线程甲在检测标志期间，不断睡眠`std::this_thread::sleep_for`，相比而言没有浪费处理事件，比上面的策略好。但是睡眠时间间隔需要谨慎设置，太短会频繁查验，太长会造成延时。
- 使用条件变量，可以规避以上做法的缺点。

条件变量：
- `std::condition_variable`只能配合`std::unique_lock<std::mutex>`，开销相对小，应优先采用。
- `std::condition_variable_any`可以配合任何互斥锁，有一些额外开销。
- 通知方：
    - `notify_one notify_all`
- 等待方：
    - `wait wait_for wait_until`
    - 使用带谓词版本会同时检测谓词是否成立，可以确保不是假醒。
    - 不带谓词版本可能假醒。
- 需要和一个互斥锁配合，必须在获得互斥锁的情况下等待，等待过程中会释放互斥锁阻塞，等到通知后重新锁定。

使用条件变量构建通用的线程安全的队列：见[P78.ThreadSafeQueue.cpp](P78.ThreadSafeQueue.cpp)。

## 使用future等待一次性事件发生

可以使用`std::future`来等待一次性发生的事件：
- 如果时间发生，那么`std::future`则会获得结果。
- 如果不需要结果，那么可以使用`std::future<void>`。
- 一旦时间发生，其共享状态进入就绪状态，不能再进行第二次等待。
- `std::future`只能让一个线程等待事件，`std::shared_future`可以让多个线程等待，但无论如何都只能等待一次。
- `std::future`对象应该仅用于一个线程中，其本身并不提供同步，多个线程访问同一个时应该外部进行同步（通常来说不应该这么做，而应该改用`std::shared_future`）。

从后台任务返回值：
- `std::thread`并未提供从计算任务返回值的机制，所以我们需要使用`std::async`或者`std::packaged_task`。
- 如果不用基于任务的并发方式的话就只有将`std::future`对应的`std::promise`传入线程启动的函数内，通过其`set_value/set_exception/...`等接口设置结果。
- 应用例子：
    - `std::packaged_task`可以作为表示可调用对象表示一个个事件传递到GUI线程中予以执行，在其他线程获取`std::future`，如果需要结果则可以调用`get`，不需要则可以抛弃这个`std::future`对象。
    - 在大批量网络连接，一个线程表示一个连接可能导致系统吃不消创建和调度开销，可以在一个线程中处理多个连接，将大量`std::promise`包装在其中处理后，外部通过对应的`std::future`获取处理结果（是否发送成功、成功接收了多少数据等信息）。

异常：
- `std::promise/std::future`提供了便捷的异常处理，异步调用抛的异常将被原样保存在共享状态中，并在`std::future::get`调用时重新抛出。
- 在不调用`std::promise`的设置函数，也不执行包装的任务，而直接销毁与`std::future`关联的`std::promise/std::packaged_task`时，其析构函数会将异常（`std::future_error`带有`std::broken_promise`错误码）保存到共享状态中。

多个线程一起等待：
- 如果是一对一地在线程间传递数据，那么`std::future`都能处理，但是如果是多个线程接收数据，就不行了。
- 多个线程不应该同时访问一个`std::future`，而应该使用`std::shared_future`。况且`std::future`对异步调用是独占的，其`get`只能调用一次，并发访问是没有意义的。
- `std::shared_future`的所有实例均指向同一异步任务的共享状态。
- 即便是改用了`std::shared_future`，如果是对同一个`std::shared_future`对象的访问依然没有同步。应该在每个线程内保存一份自己的`std::shared_future`副本，他们指向同一个共享状态。
- 如果是有一个共享的`std::shared_future gsf;`可以在每个线程本地复制一个：`auto local = gsf;`。
- 可以使用`std::future::share`或者`std::future`移动构造`std::shared_future`。

## 限时等待

C++11时间库：
- 时钟类型：
    - `std::chrono::steady_clock`恒稳时钟，不随系统时间改变而改变。一般用于不需要壁挂时间的所有地方。`static is_steady`成员表示其是否恒稳。
    - `std::chrono::system_clock`系统壁挂时钟，随系统时间改变而改变。一般只在获取壁挂时间时才用。
    - `std::chrono::high_resolution_clock`高精度时钟，实现中一般只是前面两者的别名，所以一般来说不用。
    - `time_point`嵌套类型得到当前时钟时间点，`now`静态函数返回当前时间点。
- 时间点：
    - `std::time_point<Clock, Duration = typename Clock::Duration>`
    - 第一个模板参数表示所属时钟。
    - 成员类型：
        - `rep`：用于计数的算术类型。
        - `period`：表示单位的`std:ratio`类型。
    - 共享同一时钟的两个时间点可以加减，得到时间段。
- 时长（时间段）：
    - `std::chrono::duration<Rep, Period>`，第一个模板参数是计数的算术类型，第二个是表示单位的分数。
    - 分钟单位为`std::ration<60, 1>`，毫秒则为`std::ratio<1, 1000>`。
    - 预设时长单位：`nanoseconds microseconds milliseconds seconds minutes hours`，C++20起还有`days weeks months years`。
    - 对应`std::chrono_literals`内的字面量运算符：`ns us ms s min h d w d y`。
    - `count`获取时长计数值。
    - 时长类型转换：`std::chrono::duration_cast<TargetDurationType>(d)`

提供超时时限的接口：
- `_for`接口接受时间段，`_until`接口接受时间点（为了规避系统时间调整的影响，都应该使用恒稳时钟的时间点）。
- `namespace std::this_thread`: `sleep_for sleep_until`
- `std::condition_variable/_any`: `wait_for wait_until`
- `std::timed_mutex/std::recursive_timed_mutex/std::shared_timed_mutex`: `try_lock_for try_lock_until`
- `std::shared_timed_mutex`: `try_lock_shared_for try_lock_shard_until`
- `std::unique_lock`: `try_lock_for try_lock_until`
- `std::shared_lock`: `try_lock_for try_lock_until`
- `std::future/std::shared_future`: `wait_for wait_until`

## 运用同步操作简化代码

利用`future`进行函数式编程：
- 函数式编程凭借无状态的纯函数，没有数据共享，没有副作用，在并发时也就不需要任何同步措施，这令函数式编程天然就线程安全。
- C++是多范式的，当然也可以进行函数式编程，利用`std::future`调用无副作用的纯函数，可以使一个任务依赖另一个异步任务的结果，但却不需要显式访问共享数据。
- 一个函数式编程风格的并行快速排序：
```C++
template<typename T>
std::list<T> parallelQuickSort(std::list<T> input)
{
    if (input.empty())
    {
        return input;
    }
    std::list<T> result;
    result.splice(result.begin(), input, input.begin());
    const T& pivot = *result.begin();
    auto dividePoint = std::partition(input.begin(), input.end(),
        [&](const T& t) { return t < pivot; });
    std::list<T> lowerPart;
    lowerPart.splice(lowerPart.begin(), input, input.begin(), dividePoint);
    std::future<std::list<T>> newLowerPart = std::async(&parallelQuickSort<T>, std::move(lowerPart));
    auto newHigherPart = parallelQuickSort(std::move(input));
    result.splice(result.end(), newHigherPart);
    result.splice(result.begin(), newLowerPart.get());
    return result;
}
```

使用消息传递进行同步：
- 除了函数式编程风格，还有其他并发编程风格可以选择，通信式串行进程（Communicating Sequential Process, CSP）也是一种选择：按照设计概念，CSP线程完全隔离，没有共享数据，采用通信管道传递信息。
- 只要遵守一些准则，C++同样可以支持CSP范式。
- CSP的理念很简单：假设不存在共享数据，线程只通过消息队列接受消息，每个CSP线程实际上等效于一个状态机：从原始状态开始，受到消息后以某种方式更新自身状态，也许还会向其他线程发送消息。
- 这种范式是否合适，还要看具体场景。
- 真正的CSP模型没有共享数据，全部通信通过消息队列传递，但是C++共享地址空间，因而这一规定无法强制实施。所以使用时必须依靠恪守规定，确保提出线程间的共享数据。
- 作为线程间通信的唯一途径，消息队列必须要共享，但是细节需要由程序库隐藏。

符合并发技术规约的后续风格并发：
- 并发技术规约位于名字空间`std::experimental`，属于实验特性，其中有`std::promise/std::packaged_task/std::future`的新版本。
- 这个新版`std::future`提供一个`then(continuation)`函数，一旦前一个任务完成，结果会作为后续任务`continuation`的参数，调用后前一个`std::future`随即失效，返回一个新的`future`，实现连锁调用。
- 异常的可以在`future`之间传播，从一个传递到下一个。
- 我的环境中不支持`<experimental/future>`头文件，例子略。

等待多个`future`：
- 如果我们将任务划分为多个子任务并发异步执行，通过`std::future`等待结果。那么可能会有一些任务计算完成而其他没有计算完成，导致当前线程频繁唤醒但是又频繁阻塞，导致多余的上下文开销。
- 就算将等待与合并结果的任务异步到另一个线程也还是这样，只是等待频繁唤醒和阻塞在另一个线程罢了。
- 这种“等待-切换”过程是可以避免的，通过使用`std::experimental::when_all()`。
- 用法：`std::experimental::when_all(futureIterBegin, futureIterEnd)`。
- 同样还有类似的`std::experimental::when_any`当任何一个就绪就能够就绪，只要一个完成运行就马上处理。
- 环境不支持，例子略。
- 目前来说，都不可用。

使用线程闩`std::latch`和线程屏障`std::barrier`：
- C++20已经从并发扩展并入标准，位于头文件`<latch> <barrier>`。
- 线程闩`std::latch`是一个同步对象，内含一个计数器，对线程加闩可以让其等待，多个线程都可以对其减持多次，减到0时，等待的线程会进入就绪状态，并且以后一直保持就绪状态。因此线程闩是一个轻量级的一次性工具，用于等待一系列目标事件发生。
- 线程屏障`std::barrier`用于多个线程（称为一个组）中，运行到线程屏障时会阻塞，一直等到同组线程全部抵达，那个瞬间，所有线程都会被释放。线程屏障可以被重新使用。

`std::latch`：
- 构造：`std::latch(expected)`。
- 不可拷贝不可移动。
- `count_down(n)`原子递减计数。
- `wait()`等待计数减到0。
- `try_wait()`不阻塞测试计数是否减到0，有非常低的概率在减到0时返回`false`。
- `arrive_and_wait(n)`：原子地递减并阻塞直到减到0，等价于`count_down(n); wait();`。

`std::barrier`：
- 构造：`std::barrier<> br(n)`，传入同步组中的线程数量。
- `arrive(n)`：递减n个计数，并且得到一个`arrival_token`（同步点对象），将它传给`wait`以等待同步组中当前轮次的所有线程到达屏障。
- `wait(token)`等待这个token（如果是当前轮次的同步点，则阻塞等待，如果是前面轮次的同步点，则立即返回），仅接受右值。
- `arrive_and_wait()`等价于`wait(arrive())`递减1并且阻塞等待线程到达屏障。
- `arrive_and_drop()`将会使当前阶段期待计数减一，并且使所有后续初始期待计数减一（也就是从同步组中移除一个线程，其实同步组是用这个数量来管理的，并非关联到具体线程）。
