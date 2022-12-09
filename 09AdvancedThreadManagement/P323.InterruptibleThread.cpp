#include <iostream>
#include <iomanip>
#include <thread>
#include <mutex>
#include <future>
#include <atomic>
#include <stdexcept>
#include <exception>
#include <chrono>

using namespace std::chrono_literals;

class InterruptFlag
{
    std::atomic<bool> flag {false};
public:
    void set()
    {
        flag = true;
    }
    bool isSet()
    {
        return flag.load();
    }
};

class InterruptibleThread
{
    std::thread internalThread;
    InterruptFlag* flag {};
public:
    inline static thread_local InterruptFlag thisThreadInterruptFlag;

    template<typename FunctionType>
    InterruptibleThread(FunctionType f) : flag(nullptr)
    {
        std::promise<InterruptFlag*> p;
        internalThread = std::thread([f, &p] {
                p.set_value(&InterruptibleThread::thisThreadInterruptFlag);
                f();
            });
        flag = p.get_future().get();
    }
    ~InterruptibleThread()
    {
        if (internalThread.joinable())
        {
            internalThread.join();
        }
    }
    void join()
    {
        internalThread.join();
    }
    void detach()
    {
        internalThread.detach();
    }
    bool joinable()
    {
        return internalThread.joinable();
    }
    void interrupt()
    {
        if (flag)
        {
            flag->set();
        }
    }
};

class ThreadInterrupted : public std::exception
{
public:
    virtual const char* what() const noexcept override
    {
        return "thread interrupted!";
    }
};

// check if there is a interruption
void interruptionPoint()
{
    if (InterruptibleThread::thisThreadInterruptFlag.isSet())
    {
        throw ThreadInterrupted();
    }
}

int main(int argc, char const *argv[])
{
    auto f = []() {
        try
        {
            for (int i = 0; i < 100; i ++)
            {
                std::cout << i << std::endl;
                interruptionPoint();
                std::this_thread::sleep_for(10ms);
            }
        }
        catch(ThreadInterrupted& e)
        {
            std::cerr << e.what() << '\n';
        }
    };
    InterruptibleThread t(f);
    std::this_thread::sleep_for(200ms);
    t.interrupt();
    return 0;
}
