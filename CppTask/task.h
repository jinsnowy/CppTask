#pragma once
#include <iostream>
#include <tuple>
#include <future>
#include <thread>
#include <memory>
#include <atomic>

#include "function_traits.h"

namespace tpl
{
    template<typename F, typename ...Args>
    struct func_wrapper : callable_t<std::decay_t<typename gen_task_type<F, Args...>::Return>>
    {
        using ThisType = func_wrapper<F, Args...>;
        using FuncType = typename gen_task_type<F, Args...>::Func;
        using TupleType = typename gen_task_type<F, Args...>::Tuple;
        using ReturnType = std::decay_t<typename gen_task_type<F, Args...>::Return>;
        using BaseType = callable_t<ReturnType>;

        FuncType func;
        TupleType params;

        func_wrapper(F&& f, Args&&... args) 
            : 
            func(std::forward<F>(f)),
            params(std::forward<Args>(args)...) 
        {
        }

        ReturnType operator()() const override {
            return const_cast<ThisType*>(this)->call_fn(std::make_index_sequence<std::tuple_size_v<TupleType>>()); 
        }

    private:
        template<size_t ...Is>
        ReturnType call_fn(std::index_sequence<Is...>) {
            return func(std::get<Is>(std::forward<TupleType>(params))...);
        }
    };

    template<typename F, typename ...Args>
    static auto make_func_wrapper(F&& f, Args&&... args) { return func_wrapper<F, Args...>(std::forward<F>(f), std::forward<Args>(args)...); }

    template<typename F, typename ...Args>
    static typename func_wrapper<F, Args...>::BaseType* make_func_wrapper_pointer(F&& f, Args&&... args) { return new func_wrapper<F, Args...>(std::forward<F>(f), std::forward<Args>(args)...); }

    enum task_status : unsigned char
    {
        created,
        running,
        completed,
        canceled,
        faulted,
    };

    template<typename T>
    struct control_block
    {
        task_status	status;
        std::atomic<bool> dispatch_once;
        std::future<void> dispatch_token;

        std::promise<T> result_token_setter;
        std::future<T> result_token;

        control_block() 
            : 
            status(created),
            dispatch_once(false),
            result_token_setter{},
            result_token(result_token_setter.get_future())
        {}

        bool is_dispatchable() {
            bool expected = false; 
            return dispatch_once.compare_exchange_strong(expected, true, std::memory_order::memory_order_acquire); 
        }

        void set_dispatched(std::future<void>&& dispatch_token_in) {
            status = running; 
            dispatch_token = std::move(dispatch_token_in); 
        }
    };

    template<typename T>
    struct task_awaiter;

    template<typename T>
    class task_base
    {
    protected:
        std::shared_ptr<callable_t<T>> callable;
        std::shared_ptr<control_block<T>> signal;

        task_base(callable_t<T>* callableIn)
            :
            callable(callableIn),
            signal(std::make_shared<control_block<T>>())
        {
        }
    public:
        task_base(const task_base& rhs) {
            callable = rhs.callable;
            signal = rhs.signal;
        };

        task_base& operator=(const task_base& rhs) {
            callable = rhs.callable;
            signal = rhs.signal;
            return *this;
        };

        //task_base(task_base&&) = default;
        //task_base& operator=(task_base&&) = default;

        virtual void operator()() = 0;

        void wait() {
            signal->result_token.wait();
        }

        task_status get_status() const { return signal->status; }

        task_awaiter<T> get_awaiter() const { return { signal }; }
    };

    template<typename T>
    class task : public task_base<T>
    {
    public:
        task(callable_t<T>* callableIn) 
            : 
            task_base<T>(callableIn)
        {
        }

        virtual void operator()() override {
            try {
                T&& value = (*task_base<T>::callable)();
                task_base<T>::signal->status = completed;
                task_base<T>::signal->result_token_setter.set_value(std::forward<T>(value));
            }
            catch (const std::exception&) {
                task_base<T>::signal->status = faulted;
                task_base<T>::signal->result_token_setter.set_exception(std::current_exception());
            }
        }

        void start() {
            if (task_base<T>::signal->is_dispatchable()) {
                task_base<T>::signal->set_dispatched(std::async(std::launch::async, [clone_task = *this]() mutable { (clone_task)(); }));
            }
            else {
                throw std::logic_error("task already started");
            }
        }

        T result() { return task_base<T>::signal->result_token.get(); }
    };

    template<>
    class task<void> : public task_base<void>
    {
    public:
        task(callable_t<void>* callableIn) 
            : 
            task_base<void>(callableIn)
        {}

        virtual void operator()() override {
            try {
                (*callable)();
                signal->status = completed;
                signal->result_token_setter.set_value();
            }
            catch (const std::exception&) {
                signal->status = faulted;
                signal->result_token_setter.set_exception(std::current_exception());
            }
        }

        void start() {
            if (signal->is_dispatchable()) {
                signal->set_dispatched(std::async(std::launch::async, [clone_task = *this]() mutable { clone_task(); }));
            }
            else {
                throw std::logic_error("task already started");
            }
        }
    };

    template<typename T>
    struct task_awaiter {
        std::shared_ptr<control_block<T>> signal;

        task_awaiter(const std::shared_ptr<control_block<T>>& signalIn) 
            :
            signal(signalIn) 
        {
        }

        bool is_completed() {
            return signal->status > running; 
        }

        T get_result() {
            return signal->result_token.get(); 
        }
    };

    template<typename F, typename ...Args>
    static inline auto make_task(F&& f, Args&&... args)
    {
        return task<typename func_wrapper<F, Args...>::ReturnType>(make_func_wrapper_pointer(std::forward<F>(f), std::forward<Args>(args)...));
    }

    template<typename F, typename ...Args>
    static inline auto run_async(F&& f, Args&&... args)
    {
        auto task_obj = task<typename func_wrapper<F, Args...>::ReturnType>(
            make_func_wrapper_pointer(std::forward<F>(f), std::forward<Args>(args)...)
        );
        task_obj.start();
        
        return task_obj;
    }
}