<!-- START doctoc generated TOC please keep comment here to allow auto update -->
<!-- DON'T EDIT THIS SECTION, INSTEAD RE-RUN doctoc TO UPDATE -->
**Table of Contents**  *generated with [DocToc](https://github.com/thlorenz/doctoc)*

- [第二章：线程管控](#%E7%AC%AC%E4%BA%8C%E7%AB%A0%E7%BA%BF%E7%A8%8B%E7%AE%A1%E6%8E%A7)
  - [向线程传递参数](#%E5%90%91%E7%BA%BF%E7%A8%8B%E4%BC%A0%E9%80%92%E5%8F%82%E6%95%B0)
  - [移交线程归属权](#%E7%A7%BB%E4%BA%A4%E7%BA%BF%E7%A8%8B%E5%BD%92%E5%B1%9E%E6%9D%83)
  - [在运行时选择线程数量](#%E5%9C%A8%E8%BF%90%E8%A1%8C%E6%97%B6%E9%80%89%E6%8B%A9%E7%BA%BF%E7%A8%8B%E6%95%B0%E9%87%8F)
  - [识别线程](#%E8%AF%86%E5%88%AB%E7%BA%BF%E7%A8%8B)

<!-- END doctoc generated TOC please keep comment here to allow auto update -->

# 第二章：线程管控

## 向线程传递参数

向线程传递参数的形式：`std::thread(f, args...)`
- 参数会被原样传递，每个参数完美转发并做一次拷贝得到`f_copy`和`args_copy...`，如果传入右值且有移动构造那么就是移动。这一次拷贝或者移动发生在当前线程内。
- 然后开了新线程之后再从这些拷贝将参数移动之后进行调用：`INVOKE(std::move(f_copy), std::move(args_copy)...)`，其中`INVOKE`一般是`std::invoke`。这次调用、参数传递等行为都发生在新线程。
- 所以：
    - 可调用对象在拷贝和移动前后一定要行为相同，不然行为就不会如预期那样。
    - 传递给`thread`的指针类型参数：
        - 如果`f`也是接受指针，那么数据会被共享，确保新线程使用指针期间指针不会失效。
        - 如果`f`不接受指针而是将指针转换为新类型，比如传递给`thread`的是`const char*`，而`f`接受`std::string`，这也是合法的，但是不要这样做。这可能会造成新线程中接收参数时该指针已失效（可能会发生在调用`detach`时，如果调用`join`则不会）。最好是在传递给`std::thread`时就显式构造`std::string`临时对象，这样会在当前线程做一次移动操作，新线程中调用时再做一次移动，完全不会有任何风险。
    - `f`接受非`const`引用参数：
        - 调用`f_copy`时肯定是右值，所以如果直接给`thread`传递该类型对象，那么是编不过的。
        - 所以对于非`const`引用必须使用`std::ref`传递，当前线程中拷贝`std::reference_wrapper`并在新线程中转换为`T&`。
        - 同样需要注意引用失效问题，保证新线程存在期间该变量不被析构并且注意避免数据竞争。
    - `f`接受`const`引用参数：
        - 同样要避免指针转换到该对象。
        - 在不用`std::ref std::cref`的情况下该参数引用的都是一个独立的临时对象，不会有数据竞争。
- 总体来说：
    - 只有使用`std::ref`然后按引用传递给`f`、和`f`接收指针参数时会造成数据通过参数共享。
    - 其他情况新线程中使用的参数要么是拷贝或移动过去的新对象（值传递）、或者引用的临时对象（`const`引用传递）。
    - 避免传递给`std::thread`指针，但是最后却转换为其他类型，会有指针失效的风险（在调用`detach`的情况下才会）。此时应总是在传递给`std::thread`时构造临时对象，会造成两次移动，并不会有多大性能损失。但实际上传递字符串字面量（`const char*`）作为`std::string`的参数不会有任何风险，传局部的`char*`buffer则有可能有风险。
- `std::thread`构造函数和`std::bind`使用差不多的机制。

可调用对象：
- 成员函数指针也可以作为可调用对象，需要一个对应类型的对象或者指针作为第一个参数。
- 需要保证赋值、移动后行为一致。

## 移交线程归属权

`std::thread`是操作系统线程的句柄，可以移动不可复制。某些时候需要移交线程归属权就需要凭借移动操作。
- 只要一个`std::thread`还管理着一个线程，就不能向其移动赋值。

## 在运行时选择线程数量

- `std::thread::hardware_concurrency`可以作为一个指标，表示最大硬件线程数量。
- 同时也要有但个线程最小运行数据量并由此得到一个数据可以指定的最大线程数。
- 取两者中较小者作为动态的线程数量，保证多线程是值得的，并且不会超过硬件线程。

## 识别线程

通过`std::thread::id`：
- 可以通过`std::thread::get_id`，通过`std::thread`对象获取。
- 通过`std::this_thread::get_id`获取当前运行的线程的id。
- `std::thread::id`定义了完善的比较运算，可以用来排序，判等，作为关联容器的键。