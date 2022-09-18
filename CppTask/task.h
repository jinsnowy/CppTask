#pragma once
#include <iostream>
#include <tuple>
#include <type_traits>
#include <functional>
#include <future>

namespace CppTask
{
    struct non_copyable
    {
        non_copyable() = default;
        ~non_copyable() = default;

        non_copyable(const non_copyable&) = delete;
        non_copyable& operator=(const non_copyable&) = delete;
    };

    template<typename... Ts> struct make_void { typedef void type; };
    template<typename... Ts> using void_t = typename make_void<Ts...>::type;

    struct callable_t 
    { 
        virtual void operator()() const = 0; 
    };

    template <typename T, typename = void>
    struct function_traits : function_traits<std::decay_t<T>> {};

    template <typename R, typename... A>
    struct function_traits<R(A...)>
    {
        using ReturnType = R;
        using ClassType = void;
        using ArgsType = std::tuple<A...>;
        using FArgsType = std::tuple<A...>;
    };

    template <typename R, typename... A>
    struct function_traits<R(*)(A...)>
    {
        using ReturnType = R;
        using ClassType = void;
        using ArgsType = std::tuple<A...>;
        using FArgsType = std::tuple<A...>;
    };

    template <typename R, typename C, typename... A>
    struct function_traits<R(C::*)(A...)>
    {
        using ReturnType = R;
        using ClassType = C;
        using ArgsType = std::tuple<A...>;
        using FArgsType = std::tuple<C*, A...>;
    };

    template <typename R, typename C, typename... A>
    struct function_traits<R(C::*)(A...) const>
    {
        using ReturnType = R;
        using ClassType = C;
        using ArgsType = std::tuple<A...>;
        using FArgsType = std::tuple<const C*, A...>;
    };

    template <typename T>
    struct function_traits<T, void_t<decltype(&T::operator())>>
        : public function_traits<decltype(&T::operator())> // for lambda
    {
        using FArgsType = typename function_traits<decltype(&T::operator())>::ArgsType;
    };

    template<typename R, typename T>
    struct gen_func_type
    {
        using type = void;
    };

    template<typename R, typename ...Args>
    struct gen_func_type<R, std::tuple<Args...>>
    {
        using type = std::function<R(Args...)>;
    };

    template<typename T>
    struct gen_param_type
    {
        using type = std::remove_reference_t<std::remove_cv_t<T>>;
    };

    template<typename T>
    using gen_param_type_t = typename gen_param_type<T>::type;

    template<typename F, typename ...Args>
    struct gen_task_type
    {
        using R = typename function_traits<std::decay_t<F>>::ReturnType;
        using FArgs = typename function_traits<std::decay_t<F>>::FArgsType;
        using Func = typename gen_func_type<R, FArgs>::type;
        using Tuple = std::tuple<gen_param_type_t<Args>...>;
    };

    template<typename F, typename ...Args>
    struct task_wrapper : callable_t {
        using ThisType = task_wrapper<F, Args...>;
        using FuncType = typename gen_task_type<F, Args...>::Func;
        using TupleType = typename gen_task_type<F, Args...>::Tuple;
        using ReturnType = typename gen_task_type<F, Args...>::R;

        FuncType func;
        TupleType params;

        task_wrapper(F&& f, Args&&... args) : func(std::forward<F>(f)), params(std::forward<Args>(args)...) {}

        void operator()() const override { const_cast<ThisType*>(this)->call_fn(std::make_index_sequence<std::tuple_size_v<TupleType>>()); }

        ReturnType call() const { return const_cast<ThisType*>(this)->call_fn(std::make_index_sequence<std::tuple_size_v<TupleType>>()); }

    private:
        template<size_t ...Is>
        ReturnType call_fn(std::index_sequence<Is...>) { return func(std::get<Is>(std::forward<TupleType>(params))...); }
    };

    template<typename F, typename ...Args>
    static auto make_task_wrapper(F&& f, Args&&... args) 
    { 
        return task_wrapper<F, Args...>{std::forward<F>(f), std::forward<Args>(args)...};
    }

    template<class T>
    struct task;

    template<typename T>
    struct task_base {};

    template<typename R, typename D>
    struct task_holder;

    template<typename R>
    struct task
    {
        using ReturnType = R;

        std::function<R()> task_obj;

        template<typename F, typename ...Args>
        task(F&& funcIn, Args&&... paramsIn)
            :
            task_obj(
                [t_obj = task_wrapper<F, Args...>(std::forward<F>(funcIn), std::forward<Args>(paramsIn)...)]()
                -> R
        {
            return t_obj.call();
        })
        {
        }

        R operator()() const 
        {
            return task_obj();
        }

        template<typename F, typename = std::enable_if_t<std::is_assignable_v<typename function_traits<F>::FArgsType, R>>>
        auto then(F&& func)
        {
            using ResultType = typename function_traits<F>::ReturnType;
            return task<ResultType>([this_task = std::move(this->task_obj), f = std::forward<F>(func)]()
            {
                return f(this_task());
            });
        }

        template<typename D>
        auto dispatch()
        {
            return D::push_task([this_task = std::move(this->task_obj)]() -> R {
                return this_task();
            });
        }

        task(task&& rhs) noexcept { task_obj = std::move(rhs.task_obj); }
        task& operator=(task&& rhs) noexcept { task_obj = std::move(rhs.task_obj); return *this; }
    };

    template<>
    struct task<void>
    {
        using ReturnType = void;

        std::function<void()> task_obj;

        template<typename F, typename ...Args>
        task(F&& funcIn, Args&&... paramsIn)
            :
            task_obj(
                [t_obj = task_wrapper<F, Args...>(std::forward<F>(funcIn), std::forward<Args>(paramsIn)...)]()
        {
            t_obj();
        })
        {
        }

        void operator()() const
        {
            task_obj();
        }

        template<typename F, typename = std::enable_if_t<std::is_same_v<typename function_traits<F>::FArgsType, std::tuple<>>>>
        auto then(F&& func)
        {
            using ResultType = typename function_traits<F>::ReturnType;
            return task<ResultType>([this_task = std::move(this->task_obj), f = std::forward<F>(func)]() -> ResultType
            {
                this_task();
                return f();
            });
        }

        template<typename D>
        auto dispatch()
        {
            return D::push_task([this_task = std::move(this->task_obj)](){
                this_task();
            });
        }

        task(task&& rhs) noexcept { task_obj = std::move(rhs.task_obj); }
        task& operator=(task&& rhs) noexcept { task_obj = std::move(rhs.task_obj); return *this; }
    };
    
    template<typename F, typename ...Args>
    static auto make_task(F&& f, Args&&... args)
    {
        using ResultType = typename function_traits<F>::ReturnType;
        return task<ResultType>(std::forward<F>(f), std::forward<Args>(args)...);
    }

    template<typename T>
    class task_worker : non_copyable
    {
    public:
        friend class task_processor;

        static T& get() { static T worker; return worker; }

    protected:
        virtual void start() {};

        virtual void wait_all() {};

        virtual void stop() {};
    };

    struct revocable_t
    {
        struct Impl {
            friend class cancellation_token;
            friend class cancellation_token_source;
        private:
            atomic<bool> canceled;
            bool is_canceled()const { return canceled; }
            void set_cancel() { canceled = true; }
        };

        std::shared_ptr<Impl> inst;
        revocable_t() : inst(std::make_shared<Impl>()) {}
    };

    struct cancellation_token_source;
    struct cancellation_token {
        static const cancellation_token empty_token;

        std::shared_ptr<revocable_t::Impl> inst;

        cancellation_token() {}
        cancellation_token(const cancellation_token_source& src) : inst(src.inst) {}

        bool empty() const { return inst == nullptr; }
        bool is_canceled() const { if (empty()) return false; return inst->is_canceled(); }
    };

    const cancellation_token cancellation_token::empty_token = {};

    struct cancellation_token_source
    {
        std::shared_ptr<revocable_t::Impl> inst;

        cancellation_token_source() : inst(std::make_shared<revocable_t::Impl>()) {}
        cancellation_token get_token() { return { *this }; }
        bool is_canceled() const { return inst->is_canceled(); }
        void set_cancel() { inst->set_cancel(); }
    };

    template<typename T>
    struct future_t {};

    template<typename T, typename D>
    struct dispatchable_t : future_t<T>
    {
        virtual T get() = 0;

        template<typename F, typename = std::enable_if_t<std::is_assignable_v<std::tuple<T>, typename function_traits<F>::FArgsType>>>
        auto continue_with(F&& func)
        {
            return D::push_task([this, f = std::forward<F>(func)]() -> typename function_traits<F>::ReturnType
            {
                return f(get());
            });
        }

        template<typename T>
        auto continue_with(task<T>& continued_task)
        {
            return D::push_task([this, t_obj = std::move(continued_task)]() ->T
            {
                get(); t_obj();
            });
        }
    };

    template<typename D>
    struct dispatchable_t<void, D> : future_t<void>
    {
        virtual void get() = 0;

        template<typename F, typename = std::enable_if_t<std::is_assignable_v<std::tuple<>, typename function_traits<F>::FArgsType>>>
        auto continue_with(F&& func)
        {
            return D::push_task([this, f = std::forward<F>(func)]() -> typename function_traits<F>::ReturnType
            {   
                get(); return f();
            });
        }

        template<typename T>
        auto continue_with(task<T>& continued_task)
        {
            return D::push_task([this, t_obj = std::move(continued_task)]() -> T
            {
                get(); t_obj();
            });
        }
    };

    template<typename T>
    struct awaitable_impl_t
    {
        std::promise<T> p;
        std::future<T> f;

        awaitable_impl_t() : p(), f(p.get_future()) {}
    };

    template <typename T, typename D>
    struct awaitable_t : dispatchable_t<T, D>
    {
        friend struct task_holder<T, D>;
    private:
        using Impl = awaitable_impl_t<T>;

        std::shared_ptr<Impl> pImpl;

        void set_value(T&& v) {
            pImpl->p.set_value(std::forward<T>(v));
        }

    public:
        awaitable_t() : pImpl(std::make_shared<Impl>()) {}

        T get() override {
            return pImpl->f.get();
        }

        void wait() {
            pImpl->f.wait();
        }
    };

    template <typename D>
    struct awaitable_t<void, D> : dispatchable_t<void, D>
    {
        friend struct task_holder<void, D>;
    private:
        using Impl = awaitable_impl_t<void>;

        std::shared_ptr<Impl> pImpl;

        void set_value() {
            pImpl->p.set_value();
        }
    public:
        awaitable_t() : pImpl(std::make_shared<Impl>()) {}

        void get() {
            pImpl->f.get();
        }

        void wait() {
            pImpl->f.wait();
        }
    };

    template<typename T, typename D>
    struct future_wrapper : dispatchable_t<T, D>
    {
    private:
        std::shared_ptr<std::future<T>> pImpl;

    public:
        future_wrapper(std::future<T>&& fut) : pImpl(std::make_shared<std::future<T>>(std::move(fut))) {}

        T get() { return pImpl->get(); }

        void wait() { pImpl->wait(); }
    };

    template<typename D>
    struct future_wrapper<void, D> : dispatchable_t<void, D>
    {
    private:
        std::shared_ptr<std::future<void>> pImpl;

    public:
        future_wrapper(std::future<void>&& fut) : pImpl(std::make_shared<std::future<void>>(std::move(fut))) {}

        void get() { pImpl->get(); }

        void wait() { pImpl->wait(); }
    };

    template<typename T, typename D>
    struct task_awaiter : awaitable_t<T, D>
    {
    };

    template<typename T, typename D>
    struct task_awaiter<std::future<T>, D> : future_wrapper<T, D>
    {
        task_awaiter(std::future<T>&& fut) : future_wrapper<T, D>(std::move(fut)) {}
    };
    
    template<typename R, typename D>
    struct task_holder : callable_t
    {
        task<R> inst;

        mutable task_awaiter<R, D> awaiter;

        task_holder(task<R>&& instIn) : inst(std::move(instIn)) {}

        virtual void operator()() const { awaiter.set_value(inst()); }

        task_awaiter<R, D> get_awaiter() { return awaiter; }
    };

    template<typename D>
    struct task_holder<void, D> : callable_t
    {
        task<void> inst;

        mutable task_awaiter<void, D> awaiter;

        task_holder(task<void>&& instIn) : inst(std::move(instIn)) {}

        virtual void operator()() const { inst(); awaiter.set_value(); }

        task_awaiter<void, D> get_awaiter() { return awaiter; }
    };

    struct task_t
    {
        task_t() : task_pointer(nullptr) {}

        task_t(callable_t* holder)
            :
            task_pointer(holder)
        {
        }

        ~task_t() { if (task_pointer) { delete task_pointer; } }

        void operator()() const { (*task_pointer)(); }

        task_t(const task_t&) = delete;
        task_t& operator=(const task_t&) = delete;

        task_t(task_t&& rhs) noexcept { task_pointer = rhs.task_pointer; rhs.task_pointer = nullptr; }
        task_t& operator=(task_t&& rhs) noexcept { task_pointer = rhs.task_pointer; rhs.task_pointer = nullptr; return *this; }

        callable_t* task_pointer;
    };

    template<typename T>
    struct task_future_t
    {
        task_future_t() : task_future_pointer(nullptr) {}

        task_future_t(future<T>* holder)
            :
            task_future_pointer(holder)
        {
        }

        ~task_t() { if (task_pointer) { delete task_pointer; } }

        void operator()() const { (*task_pointer)(); }

        task_t(const task_t&) = delete;
        task_t& operator=(const task_t&) = delete;

        task_t(task_t&& rhs) noexcept { task_pointer = rhs.task_pointer; rhs.task_pointer = nullptr; }
        task_t& operator=(task_t&& rhs) noexcept { task_pointer = rhs.task_pointer; rhs.task_pointer = nullptr; return *this; }

        future<T>* task_future_pointer;
    };

    static void test();
}