#pragma once
#include "task.h"

namespace CppTask
{
    class async_worker : public task_worker<async_worker>
    {
    public:
        template<typename F, typename ...Args>
        static auto push_task(F&& f, Args&&... args)
        {
            using R = typename function_traits<F>::ReturnType;

            auto t_fut = std::async(std::launch::async, std::forward<F>(f), std::forward<Args>(args)...);

            return task_awaiter<std::future<R>, async_worker>(std::move(t_fut));
        }
    };
}
