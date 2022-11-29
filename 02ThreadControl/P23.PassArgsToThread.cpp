#include <iostream>
#include <thread>
#include <string>
#include <chrono>

using namespace std::chrono_literals;

class A
{
public:
    A(int _val) : val(_val)
    {
        std::cout << "construct at thread " << std::this_thread::get_id() << ", value : " << val << std::endl;
    }
    A(const A& a) : val(a.val + 1)
    {
        std::cout << "copy at thread " << std::this_thread::get_id() << ", value : " << val << std::endl;
    }
    A(A&& a) : val(a.val + 1)
    {
        std::cout << "move at thread " << std::this_thread::get_id() << ", value : " << val << std::endl;
    }
    int val;
};


void hello(A a1, A a2, A& a3, const A& a4)
{
    std::cout << "a1 : " << a1.val << std::endl;
    std::cout << "a2 : " << a2.val << std::endl;
    std::cout << "a3 : " << a3.val << std::endl;
    std::cout << "a4 : " << a4.val << std::endl;
}

int main(int argc, char const *argv[])
{
    A a1(0);
    A a2(100);
    A a3(200);
    std::cout << std::endl;
    std::thread t(hello, -100, std::move(a1), std::ref(a2), a3);
    t.join();
    return 0;
}
