<!-- START doctoc generated TOC please keep comment here to allow auto update -->
<!-- DON'T EDIT THIS SECTION, INSTEAD RE-RUN doctoc TO UPDATE -->
**Table of Contents**  *generated with [DocToc](https://github.com/thlorenz/doctoc)*

- [第十章：并行算法](#%E7%AC%AC%E5%8D%81%E7%AB%A0%E5%B9%B6%E8%A1%8C%E7%AE%97%E6%B3%95)
  - [标准库并行算法](#%E6%A0%87%E5%87%86%E5%BA%93%E5%B9%B6%E8%A1%8C%E7%AE%97%E6%B3%95)
  - [执行策略](#%E6%89%A7%E8%A1%8C%E7%AD%96%E7%95%A5)
  - [C++标准库的并行算法](#c%E6%A0%87%E5%87%86%E5%BA%93%E7%9A%84%E5%B9%B6%E8%A1%8C%E7%AE%97%E6%B3%95)

<!-- END doctoc generated TOC please keep comment here to allow auto update -->

# 第十章：并行算法

## 标准库并行算法

C++17标准库引入了并行算法函数，他们是新引入的函数重载，操作目标都是区间。
- 相比普通版本的算法，这些函数多一个参数（放在第一个位置），其他东西都相同。
- 第一个参数是执行策略，调用并行版本是向标注库示意可以采用多线程，而不是强制命令。标准库可以自行决定该调用的运行方式。
- 并行算法会改变对复杂度的要求，比普通串行算法对复杂度要求略为宽松。
- 并行算法的工作总量往往更多。

## 执行策略

执行策略：
- `std::execution::sequenced_policy`
- `std::execution::parallel_policy`
- `std::execution::parallel_unsequenced_policy`
- `std::execution::unsequenced_policy`（C++20）
- 这四个是类，使用时需要传递下面的对象：
- `std::execution::seq`
- `std::execution::par`
- `std::execution::par_unseq`
- `std::execution::unseq`
- 这些执行策略会影响算法的行为。
- 根据标准，C++标准库实现可以提供额外的执行策略，并自行决定策略的语义。
- 使用并行执行策略时，程序员负责避免数据竞争和死锁。

执行策略产生的作用：
- 异常行为：带有并行执行策略的函数抛异常时会调用`std::terminate`终止程序，而不是传播异常。
- 算法中间步骤的执行起点和执行时机不同。

`std::execution::sequenced_policy`：
- 顺序策略，令算法函数在发起调用的线程上执行全部操作，不会发生并行。
- 但它仍然是并行执行策略，异常行为和对复杂度的影响也相同。
- 所有操作由同一个线程完成，必须服从一定次序，不存交错执行。
- 几乎没有施加内存次序限制。

`std::execution::parallel_policy`：
- 多个线程并行的基本模式，操作可以由发起调用的线程执行，也可以另外创建线程执行。
- 给定一个线程，其上操作必须服从一定次序，不得交错执行，但C++标准并未规定具体次序。
- 算法所服从内存次序可能因调用不同而不同。
- 这个策略能让多个调用同步访问共享数据（使用互斥）。

`std::execution::parallel_unsequenced_policy`：
- 非顺序并行策略，对内存次序施加最严格限制。
- 多个线程中可能乱序执行算法步骤。
- 不得以任何形式同步在线程间或者与其他线程同步数据。

`std::execution::unsequenced_policy`：
- 指示可将算法的指向向量化。

## C++标准库的并行算法

标准库绝大多数算法都有并行版本。并行版本对迭代器的限制会更强，比如很多非并行版本可以接受输入迭代器，但是并行版本只能接受前向迭代器。