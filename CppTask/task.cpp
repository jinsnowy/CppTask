#include "task.h"

using namespace std::chrono_literals;
using namespace std;

static void func()
{
    cout << "Static Function" << endl;
}

void CppTask::test()
{
 
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

    class B : public A{ };

    auto empty_tuple = make_tuple();
    const auto lbd_func = [](int c) { cout << "Lambda Function 2" << endl; };

    using Ta = function_traits<decltype(lbd_func)>::FArgsType;
    using Tb = function_traits<decltype(func)>::FArgsType;
    using Tc = function_traits<decltype(&A::func)>::FArgsType;
    using Td = function_traits<decltype(&A::func_const)>::FArgsType;
    using Te = function_traits<decltype(&A::func_class)>::FArgsType;
    using Tf = function_traits<decltype(&A::func_move_class)>::FArgsType;

    using Fa = gen_task_type<decltype(lbd_func), int&>::Func;
    using Fb = gen_task_type<decltype(func)>::Func;
    using Fc = gen_task_type<decltype(&A::func), int>::Func;
    using Fd = gen_task_type<decltype(&A::func_const), int>::Func;
    using Fe = gen_task_type<decltype(&A::func_class), int>::Func;
    using Ff = gen_task_type<decltype(&A::func_move_class), int>::Func;

    using Pa = gen_task_type<decltype(lbd_func), int&>::Tuple;
    using Pb = gen_task_type<decltype(func)>::Tuple;
    using Pc = gen_task_type<decltype(&A::func), int>::Tuple;
    using Pd = gen_task_type<decltype(&A::func_const), int>::Tuple;
    using Pe = gen_task_type<decltype(&A::func_class), A*, B*>::Tuple;
    using Pf = gen_task_type<decltype(&A::func_move_class), A*, TestClass>::Tuple;

    using Ra = function_traits<decltype(lbd_func)>::ReturnType;
    using Rb = function_traits<decltype(func)>::ReturnType;
    using Rc = function_traits<decltype(&A::func)>::ReturnType;
    using Rd = function_traits<decltype(&A::func_const)>::ReturnType;
    using Re = function_traits<decltype(&A::func_class)>::ReturnType;
    using Rf = function_traits<decltype(&A::func_move_class)>::ReturnType;
}
