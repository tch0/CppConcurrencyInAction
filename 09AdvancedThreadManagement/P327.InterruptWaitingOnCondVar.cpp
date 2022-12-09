#include <iostream>
#include <iomanip>
#include <thread>
#include <mutex>
#include <future>
#include <atomic>
#include <stdexcept>
#include <exception>
#include <condition_variable>
#include <chrono>

using namespace std::chrono_literals;

class InterruptFlag
{
    std::atomic<bool> flag {false};
    std::condition_variable* threadCond {};
    std::mutex condMutex;
public:
    void set()
    {
        flag.store(true);
        std::lock_guard lk(condMutex);
        if (threadCond)
        {
            threadCond->notify_all();
        }
    }
    bool isSet()
    {
        return flag.load();
    }
    void setConditionVariable(std::condition_variable& cv)
    {
        std::lock_guard lk(condMutex);
        threadCond = &cv;
    }
    void clearConditionVariable()
    {
        std::lock_guard<std::mutex> lk(condMutex);
        threadCond = nullptr;
    }
    struct ClearCVOnDestruction
    {
        ~ClearCVOnDestruction();
    };
};

thread_local InterruptFlag thisThreadInterruptFlag;

InterruptFlag::ClearCVOnDestruction::~ClearCVOnDestruction()
{
    thisThreadInterruptFlag.clearConditionVariable();
}

class InterruptibleThread
{
    std::thread internalThread;
    InterruptFlag* flag {};
public:

    template<typename FunctionType>
    InterruptibleThread(FunctionType f) : flag(nullptr)
    {
        std::promise<InterruptFlag*> p;
        internalThread = std::thread([f, &p] {
                p.set_value(&thisThreadInterruptFlag);
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
    if (thisThreadInterruptFlag.isSet())
    {
        throw ThreadInterrupted();
    }
}

// to replace std::condition_variable::wait
void interruptibleWait(std::condition_variable& cv, std::unique_lock<std::mutex>& lk)
{
    interruptionPoint();
    thisThreadInterruptFlag.setConditionVariable(cv);
    InterruptFlag::ClearCVOnDestruction guard; // RAII guard on InterruptFlag::threadCond
    interruptionPoint();
    cv.wait_for(lk, 1ms);
    interruptionPoint();
}

template<typename Predicate>
void interruptibleWait(std::condition_variable& cv, std::unique_lock<std::mutex>& lk, Predicate pred)
{
    interruptionPoint();
    thisThreadInterruptFlag.setConditionVariable(cv);
    InterruptFlag::ClearCVOnDestruction guard; // RAII guard on InterruptFlag::threadCond
    interruptionPoint();
    while (!thisThreadInterruptFlag.isSet() && !pred())
    {
        cv.wait_for(lk, 1ms);
    }
    std::cout << "interruption of wait!" << std::endl;
    interruptionPoint();
}

int main(int argc, char const *argv[])
{
    std::condition_variable cv;
    std::mutex m;
    auto f = [&cv, &m]() {
        try
        {
            std::cout << "hello" << std::endl;
            std::unique_lock<std::mutex> lk(m);
            interruptibleWait(cv, lk, [](){ return false; });
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
