#include <iostream>
#include <iomanip>
#include <thread>
#include <mutex>
#include <future>
#include <shared_mutex>
#include <chrono>
#include <utility>
#include <algorithm>
#include <list>
#include <vector>
#include <map>
#include <string>
#include <cassert>
#include <memory>
using namespace std::chrono_literals;

template<typename T>
class ThreadSafeList
{
private:
    struct node
    {
        std::mutex m;
        std::shared_ptr<T> data;
        std::unique_ptr<node> next;
        node() {}
        node(const T& value) : data(std::make_shared<T>(value)) {}
    };
    node head; // head do not hold any data, it's a empty node.
public:
    ThreadSafeList() {}
    ~ThreadSafeList()
    {
        removeIf([](const node&) { return true; });
    }
    ThreadSafeList(const ThreadSafeList&) = delete;
    ThreadSafeList& operator=(const ThreadSafeList&) = delete;
    void pushFront(const T& value)
    {
        std::unique_ptr<node> newNode = std::make_unique<node>(value);
        std::lock_guard<std::mutex> lk(head.m);
        newNode->next = std::move(head.next);
        head.next = std::move(newNode);
    }
    template<typename Function>
    void forEach(Function f)
    {
        node* current = &head;
        std::unique_lock<std::mutex> lk(head.m);
        while (node* const next = current->next.get())
        {
            std::unique_lock<std::mutex> nextLk(next->m);
            lk.unlock();
            f(*next->data);
            current = next;
            lk = std::move(nextLk);
        }
    }
    template<typename Predicate>
    std::shared_ptr<T> findFirstOf(Predicate p)
    {
        node* current = &head;
        std::unique_lock<std::mutex> lk(head.m);
        while (node* const next = current->next.get())
        {
            std::unique_lock<std::mutex> nextLk(next->m);
            lk.unlock();
            if (p(*next->data))
            {
                return next->data;
            }
            current = next;
            lk = std::move(nextLk);
        }
        return std::shared_ptr<T>();
    }
    template<typename Predicate>
    void removeIf(Predicate p)
    {
        node* current = &head;
        std::unique_lock<std::mutex> lk(head.m);
        while (node* const next = current->next.get())
        {
            std::unique_lock<std::mutex> nextLk(next->m);
            if (p(*next->data)) // remove
            {
                std::unique_ptr<node> oldNext = std::move(current->next);
                current->next = std::move(oldNext->next);
                nextLk.unlock();
            }
            else // do not remove
            {
                lk.unlock();
                current = next;
                lk = std::move(nextLk);
            }
        }
    }
};

std::mutex mcout; // for cout

int main(int argc, char const *argv[])
{
    std::cout.sync_with_stdio(false);

    ThreadSafeList<int> L;
    auto pushFunc = [&L]() {
        for (int i = 0; i < 100; i++)
        {
            L.pushFront(i);
            if (i % 10 == 0)
            {
                std::this_thread::sleep_for(10ms);
            }
        }
    };
    auto traverseFunc = [&L]() {
        for (int i = 0; i < 10; i++)
        {
            {
                std::lock_guard<std::mutex> lock(mcout);
                std::cout << "thread " << std::setw(2) << std::this_thread::get_id() << " : traverse : ";
                L.forEach([](int value) { 
                    std::cout << value << " ";
                });
                std::cout << std::endl;
            }
            std::this_thread::sleep_for(15ms);
        }
    };
    auto removeFunc = [&L]() {
        for (int i = 0; i < 10; i++)
        {
            std::this_thread::sleep_for(10ms);
            {
                std::lock_guard<std::mutex> lock(mcout);
                std::cout << "thread " << std::setw(2) << std::this_thread::get_id() << " : remove " << i*10+i << std::endl;
            }
            L.removeIf([i](int value) { return value == i * 10 + i; });
        }
    };
    auto findFunc = [&L]() {
        for (int i = 0; i < 10; i++)
        {
            {
                std::lock_guard<std::mutex> lock(mcout);
                std::cout << "thread " << std::setw(2) << std::this_thread::get_id() << " : find " << i*i << " : ";
                auto sp = L.findFirstOf([i](int value) { return value == i*i; });
                if (sp)
                {
                    std::cout << "found " << std::endl;
                }
                else
                {
                    std::cout << "not found " << std::endl;
                }
            }
            std::this_thread::sleep_for(11ms);
        }
    };
    std::jthread t0(pushFunc);
    std::jthread t1(traverseFunc);
    std::jthread t2(removeFunc);
    std::jthread t3(findFunc);
    return 0;
}
