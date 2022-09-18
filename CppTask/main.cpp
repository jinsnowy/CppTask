#include "task.h"
#include "single_worker.h"
#include "async_worker.h"

using namespace CppTask;
using namespace std;

class TestClass
{
public:
    TestClass() = default;
    ~TestClass() = default;

    TestClass(const TestClass& tc) { cout << "copy construct" << endl; }
    TestClass& operator=(const TestClass&) { cout << "assign construct" << endl; return *this; }

    TestClass(TestClass&& tc) noexcept { cout << "move construct" << endl; }
    TestClass& operator=(TestClass&& tc) noexcept { cout << "move assign" << endl; return *this; }
};

class B;
class A
{
public:
    void func(const int& a) { cout << "Member Function" << endl; }
    void func_const(int a) const { cout << "Const Member Function" << endl; }
    void func_class(const B* b) { cout << "Member Function Complex" << endl; };
    void func_move_class(TestClass&& tc) { cout << "Member Function With Move" << endl; }
};

class B : public A { };

void func() { cout << "static function" << endl; }

void test1();
void test2();

int main()
{
    test1();
    test2();


	return 0;
}

void test1()
{
    A a;
    TestClass tc;

	auto& worker = single_worker::get();
    single_worker::get().start();

    worker.push_task(func);
    worker.push_task([]() { for (int i = 0; i < 5; ++i) { cout << "task " << i << endl; std::this_thread::sleep_for(std::chrono::milliseconds(500)); }});
    worker.push_task(&A::func_move_class, &a, std::move(tc));

    single_worker::get().wait_all();
}

void test2()
{
    srand(time(NULL));
    int arr[10] = {};
    for (int i = 0; i < 10; ++i) arr[i] = (i+1);

    auto l = [](int&& r) { return r * r; };

    constexpr bool b3 = std::is_assignable_v<std::tuple<int>, typename function_traits<decltype(l)>::FArgsType>;

    auto& worker = async_worker::get();
    auto awaiter1 = worker.push_task([arr]() -> int { cout << "first task" << endl; return arr[rand()%10]; });
    auto awaiter2 = awaiter1.continue_with([](int r) -> int { cout << "second task num choose = " << r << endl;  return r; });
    cout << "result : " << awaiter2.get() << endl;
}