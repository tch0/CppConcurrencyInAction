<!-- START doctoc generated TOC please keep comment here to allow auto update -->
<!-- DON'T EDIT THIS SECTION, INSTEAD RE-RUN doctoc TO UPDATE -->
**Table of Contents**  *generated with [DocToc](https://github.com/thlorenz/doctoc)*

- [第五章：内存模型和原子操作](#%E7%AC%AC%E4%BA%94%E7%AB%A0%E5%86%85%E5%AD%98%E6%A8%A1%E5%9E%8B%E5%92%8C%E5%8E%9F%E5%AD%90%E6%93%8D%E4%BD%9C)
  - [内存模型基础](#%E5%86%85%E5%AD%98%E6%A8%A1%E5%9E%8B%E5%9F%BA%E7%A1%80)
  - [C++中的原子操作及其类别](#c%E4%B8%AD%E7%9A%84%E5%8E%9F%E5%AD%90%E6%93%8D%E4%BD%9C%E5%8F%8A%E5%85%B6%E7%B1%BB%E5%88%AB)
  - [同步操作和强制内存序](#%E5%90%8C%E6%AD%A5%E6%93%8D%E4%BD%9C%E5%92%8C%E5%BC%BA%E5%88%B6%E5%86%85%E5%AD%98%E5%BA%8F)

<!-- END doctoc generated TOC please keep comment here to allow auto update -->

# 第五章：内存模型和原子操作

C++11引入了新的线程感知的内存模型，精确定义了基础构建单元应当如何运转。只有当贴近计算机硬件底层细节时，内存模型的精确细节的重要作用才会彰显。
- C++是操作系统级别的编程语言，C++标准委员会的目标是令C++尽量贴近计算机硬件底层，而不必用其他更低级的语言（汇编）。
- 原子类型及其操作应运而生，提供底层同步操作功能。

## 内存模型基础

对象和内存区域：
- C++标准将对象定义为某一存储范围（a region of storage）。
    - 所以说内置类型的值也是对象，所以对象并不都具有成员函数，尽管某些语言中会这样定义。
    - 不管对象属于什么类型，都会存储在一个或多个内存区域中。每个内存连续区域是一个对象或者子对象。
    - 对于位域，需要注意，相邻位于分属不同子对象，但照样算作同一内存区域。
- 每个变量都是对象，对象的数据成员也是对象，每个对象都至少占据一块内存区域。
    - 若变量属于内建类型，无论大小都只占据一块内存区域。
    - 相邻位域属于同一内存区域。

对象、内存区域与并发：
- 所有多线程相关的事项都会牵涉到内存区域。
    - 如果两个线程各自访问分离的内存区域，则相安无事，一切运作良好。
    - 反之如果两个线程访问同一内存区域，则只要有一方更新了内存区域，那么就可能产生数据竞争。只读数据无须保护或者同步。
- 任一线程改动数据都可能引发条件竞争。
    - 要避免条件竞争，就必须强制两个线程按照一定次序访问。
    - 无论是固定不变，还是可以轮换访问次序，都必须保证访问次序清晰分明。
    - 第三章介绍过互斥可以用来保护多个线程的访问次序，同一时刻只能有一个线程访问。
    - 除此之外，还可以使用原子操作来强制两个线程具有一定的访问次序。
    - 加入多个线程访问相同内存区域，那么他们两两之间必须全部有明确的访问次序。不满足时就会导致未定义行为。

改动序列（modification order）：
- 在讨论原子操作之前，最后一个重要概念是改动序列。
- 一个C++程序中，每个对象都具有一个改动序列，有所有线程上对象的写操作构成，其中第一个写操作是初始化。
- 这个序列可以在程序的多次运行中不同，但是在程序的每一次运行中，全部线程都必须形成相同的改动序列。
- 若多个线程操作同一对象，就必须自己实施同步保证这个改动序列在多个线程中是相同的。
- 如果在多个线程中观察到同一个对象的值的序列不同，那么就说明出现了数据竞争和未定义行为。
- 如果采用了原子操作，那么由编译器负责保证改动序列相同，从而保证值序列相同。
- 为了实现这些保障，要求在某些会后禁止硬件的乱序执行与分支预测。

## C++中的原子操作及其类别

标准原子类型：
- 原子操作是不可分割的操作，线程切换上下文之前如果已经开始了一个原子操作，那么切换就不会发生在原子操作半完成时。其他线程不可能见到该线程原子操作半完成的状态。
- C++中需要通过原子类型来实现原子操作。
- 根据定义，C++内建的原子操作仅支持这些，尽管通过互斥，我们也可以令其他操作实现逻辑上的原子化。
- 而要通过互斥实现的原子操作通常达不到该有的性能提升，不如直接使用互斥。提供了`std::atomic<xxx>::is_always_lock_free`来判断一个原子类型是否是无锁实现的。
- 标准原子类型特化：`bool/char/signed char/unsigned char/int/unsigned/short/unsigned short/long/unsigned long/long long/unsigned long long/char16_t/char32_t/wchar_t`。
- C标准中提供了对应原子类型，略。
- 对于原子类型上的每一个操作，有一个重载可以提供额外参数：用于设定所需的内存次序，`std::memory_order`枚举类型。
```C++
// (since C++11)
// (until C++20)
typedef enum memory_order {
    memory_order_relaxed,
    memory_order_consume,
    memory_order_acquire,
    memory_order_release,
    memory_order_acq_rel,
    memory_order_seq_cst
} memory_order;
// since C++20
enum class memory_order : /*unspecified*/ {
    relaxed, consume, acquire, release, acq_rel, seq_cst
};
inline constexpr memory_order memory_order_relaxed = memory_order::relaxed;
inline constexpr memory_order memory_order_consume = memory_order::consume;
inline constexpr memory_order memory_order_acquire = memory_order::acquire;
inline constexpr memory_order memory_order_release = memory_order::release;
inline constexpr memory_order memory_order_acq_rel = memory_order::acq_rel;
inline constexpr memory_order memory_order_seq_cst = memory_order::seq_cst;
```
- 如果没有设定内存序，那么采用最严格的内存序`std::memory_order_seq_cst`。
- 需要内存序的操作分为三类：
    - 存储（store）操作：可选`std::memory_order_relaxed`, `std::memory_order_release`, `std::memory_order_seq_cst`。
    - 载入（load）操作：可选：`std::memory_order_relaxed`, `std::memory_order_consume`, `std::memory_order_acquire`, `std::memory_order_seq_cst`。
    - “读-改-写”（Read-Modify-Write）操作，可选：`std::memory_order_relaxed`, `std::memory_order_consume`, `std::memory_order_acquire`, `std::memory_order_release`, `std::memory_order_acq_rel`, `std::memory_order_seq_cst`。

`std::atomic_flag`：
- 最简单的原子类型，表示一个布尔标志。
- 初始化：`std::atomic_flag flag { ATOMIC_FLAG_INIT };`
- 接口：`test_and_set clear test`。
- 等待与通知：`wait notify_one notify_all`。
- 可以使用`std::atomic_flag`完美实现自旋锁，持续调用`test_and_set`即可。进而实现一个自旋锁互斥：
```C++
class SpinLockMutex
{
    std::atomic_flag flag;
public:
    SpinLockMutex() : flag{ ATOMIC_FLAG_INIT } {};
    void lock()
    {
        while (flag.test_and_set(std::memory_order_acquire))
        {
        }
    }
    void unlock()
    {
        flag.clear(std::memory_order_release);
    }
};
```
- 相比普通互斥锁，`lock`时会阻塞，自旋互斥锁加锁时会忙等待（一直不停循环），所以通常情况下并非最佳选择，特别是很多线程使用自旋互斥锁进行同步时性能会很差。但在预期只会等待很短时间（甚至少于线程切换时间时）的场景下自旋锁还是有用的。

`std::atomic<bool>`：
- 赋值操作返回值而非引用。
- 读取：`store`。
- 写入：`load`。
- “读-改-写”接口：`exchange compare_exchange_weak compare_exchange_strong`。
- 其中`compare_exchange_weak compare_exchange_strong`是比较交换操作，是原子类型的编程基石。如果完成操作返回`true`否则返回`false`。
- 其中`compare_exchange_weak`必须由一条指令完成，在**某些处理器上**（无法使用一条指令完成时）可能出现佯败，通常需要配合循环使用。
- 而`compare_exchange_strong`则保证一定成功，其自身内部有一个循环。
- 比较交换操作接收两个内存序，区分成功和失败两种情况。

`std::atomic<T*>`和整数特化：
- 在`std::atomic<bool>`基础上额外提供`fetch_add fetch_sub += -= ++ --`操作。
- 标准整数原子类型则在此基础上还额外提供`fetch_or fetch_and fetch_xor |= &= ^=`操作。
- 其他原子类型只提供`std::atomic<bool>`的基本操作。

原子操作的非成员函数：
- 见[CppReference](https://zh.cppreference.com/w/cpp/header/atomic#:~:text=atomic_is_lock_free,(C%2B%2B11))。
- 对应于所有的成员函数。
- 这些函数还对`std::shred_ptr`[提供了重载](https://zh.cppreference.com/w/cpp/memory/shared_ptr/atomic)。但在C++20已经弃用，因为[C++20起提供了`std::atomic<std::shared_ptr>`特化](https://zh.cppreference.com/w/cpp/memory/shared_ptr/atomic2)。
- 并发编程中使用共享智能指针应优先使用`std::atomic<std::shared_ptr>`和`std::atomic<std::weak_ptr>`。

## 同步操作和强制内存序

TODO。
