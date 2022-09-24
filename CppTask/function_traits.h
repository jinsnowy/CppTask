#pragma once
#include <tuple>
#include <type_traits>

namespace cpptask
{
    template<typename... Ts> struct make_void { typedef void type; };
    template<typename... Ts> using void_t = typename make_void<Ts...>::type;

    template<typename R>
    struct callable_t
    {
        virtual R operator()() const = 0;
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

    template<typename ...Args>
    struct decay_tuple_type
    {
        using type = std::tuple<Args...>;
    };

    template<typename ...Args>
    struct decay_tuple_type<std::tuple<Args...>>
    {
        using type = std::tuple<std::decay_t<Args>...>;
    };

    template<typename T>
    using gen_param_type_t = typename gen_param_type<T>::type;

    template<typename F, typename ...Args>
    struct gen_task_type
    {
        using Return = std::decay_t<typename function_traits<std::decay_t<F>>::ReturnType>;
        using FArgs = typename function_traits<std::decay_t<F>>::FArgsType;
        using Func = typename gen_func_type<Return, FArgs>::type;
        using Tuple = std::tuple<gen_param_type_t<Args>...>;
    };
}