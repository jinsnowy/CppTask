#pragma once

#include "task.h"
#include <chrono>
#include <queue>

namespace CppTask
{
    class single_worker : public task_worker<single_worker>
    {
    private:
        bool finished = false;
        std::mutex sync;
        std::condition_variable cv;
        std::thread worker;
        std::queue<task_t> tasks;

    public:
        ~single_worker() { stop(); }

        template<typename F, typename ...Args>
        static auto push_task(F&& f, Args&&... args)
        {
            using R = typename function_traits<F>::ReturnType;

            auto t_holder = new task_holder<R, single_worker>(make_task(std::forward<F>(f), std::forward<Args>(args)...));

            auto t_awaiter = t_holder->get_awaiter();

            get().enqueue(t_holder);

            return t_awaiter;
        }

        virtual void start()
        {
            worker = std::thread([this]() { work(); });
        }

        virtual void wait_all() override
        {
            cv.notify_one();

            while (1)
            {
                {
                    std::lock_guard<std::mutex> lk(sync);
                    if (tasks.empty())
                        return;
                }

                std::this_thread::sleep_for(std::chrono::milliseconds(100));
            }
        }

        virtual void stop() override
        {
            {
                std::lock_guard<std::mutex> lk(sync);
                finished = true;
            }

            cv.notify_one();

            if (worker.joinable())
            {
                worker.join();
            }
        }

    private:
        void enqueue(task_t&& task)
        {
            {
                std::lock_guard<std::mutex> lk(sync);
                tasks.emplace(std::move(task));
            }

            cv.notify_one();
        }

        void work()
        {
            while (!finished)
            {
                std::unique_lock<std::mutex> lk(sync);

                cv.wait(lk, [this]() { return !tasks.empty() || finished; });

                if (finished)
                    return;

                task_t task = std::move(tasks.front());
                tasks.pop();

                lk.unlock();

                try {
                    (task)();
                }
                catch (const std::exception& e)
                {
                    std::cout << "error : " << e.what() << std::endl;
                }
            }
        }
    };
}
