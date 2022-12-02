#include <iostream>
#include <thread>
#include <mutex>
#include <future>
#include <list>
#include <random>
#include <algorithm>

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
    std::future<std::list<T>> newLowerPart = std::async(std::launch::async, &parallelQuickSort<T>, std::move(lowerPart));
    auto newHigherPart = parallelQuickSort(std::move(input));
    result.splice(result.end(), newHigherPart);
    result.splice(result.begin(), newLowerPart.get());
    return result;
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
