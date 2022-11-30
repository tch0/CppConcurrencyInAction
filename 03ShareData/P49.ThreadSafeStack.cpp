#include <iostream>
#include <thread>
#include <mutex>
#include <exception>
#include <memory>
#include <stack>

// a thread safe stack implementation

struct empty_stack : std::exception
{
    virtual const char* what() const noexcept override
    {
        return "threadsafe_stack is empty!";
    }
};

template<typename T>
class threadsafe_stack
{
private:
    std::stack<T> data;
    mutable std::mutex m;
public:
    threadsafe_stack() {}
    threadsafe_stack(const threadsafe_stack& other)
    {
        std::lock_guard lock(other.m);
        data = other.data;
    }
    threadsafe_stack& operator=(const threadsafe_stack& other) = delete;
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
            throw empty_stack();
        }
        const std::shared_ptr<T> res(std::make_shared<T>(std::move(data.top())));
        data.pop();
        return res;
    }
    void pop(T& value)
    {
        std::lock_guard lock(m);
        if (data.empty())
        {
            throw empty_stack();
        }
        value = std::move(data.top());
        data.pop();
    }
    bool empty() const
    {
        std::lock_guard lock(m);
        return data.empty();
    }
};

int main(int argc, char const *argv[])
{
    
    return 0;
}
