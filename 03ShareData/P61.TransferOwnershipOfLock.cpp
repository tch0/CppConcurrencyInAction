#include <iostream>
#include <thread>
#include <mutex>
#include <stack>
#include <memory>

// using std::unique_lock transfer ownership of lock
template<typename T>
class ThreadSafeLock
{
private:
    std::stack<T> data;
    mutable std::recursive_mutex m;
public:
    ThreadSafeLock() {}
    ThreadSafeLock(const ThreadSafeLock& other)
    {
        std::lock_guard lock(other.m);
        data = other.data;
    }
    ThreadSafeLock& operator=(const ThreadSafeLock& other) = delete;
    void push(T new_value)
    {
        std::lock_guard lock(m);
        data.push(std::move(new_value));
    }
    std::shared_ptr<T> pop()
    {
        std::lock_guard lock(m);
        const std::shared_ptr<T> res(std::make_shared<T>(std::move(data.top())));
        data.pop();
        return res;
    }
    void pop(T& value)
    {
        std::lock_guard lock(m);
        value = std::move(data.top());
        data.pop();
    }
    T top()
    {
        std::lock_guard lock(m);
        return data.top();
    }
    bool empty(std::unique_lock<std::recursive_mutex>& inputLock) const
    {
        std::unique_lock<std::recursive_mutex> lck(m);
        inputLock = std::move(lck);
        return data.empty();
    }
};

int main(int argc, char const *argv[])
{
    ThreadSafeLock<int> s;
    s.push(0);
    {
        std::unique_lock<std::recursive_mutex> lck;
        if (!s.empty(lck)) // transfer lock to lck, now lck owns recursive_mutx of s
        {
            std::cout << "s is not empty, value : " << s.top() << std::endl;
            s.pop();
        }
        else
        {
            std::cout << "s is empty!" << std::endl;
        }
    }
    return 0;
}
