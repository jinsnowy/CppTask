#pragma once
#include <iostream>
#include <tuple>
#include <future>
#include <thread>
#include <memory>
#include <atomic>
#include <vector>

#include "function_traits.h"

namespace cpptask
{
	template<typename F, typename ...Args>
	struct func_wrapper : callable_t<typename gen_task_type<F, Args...>::Return>
	{
		using ThisType = func_wrapper<F, Args...>;
		using FuncType = typename gen_task_type<F, Args...>::Func;
		using TupleType = typename gen_task_type<F, Args...>::Tuple;
		using ReturnType = typename gen_task_type<F, Args...>::Return;
		using BaseType = callable_t<ReturnType>;

		FuncType func;
		TupleType params;

		func_wrapper(F&& f, Args&&... args) : func(std::forward<F>(f)), params(std::forward<Args>(args)...) {}

		ReturnType operator()() const override { return const_cast<ThisType*>(this)->call_fn(std::make_index_sequence<std::tuple_size_v<TupleType>>()); }

		template<size_t ...Is>
		ReturnType call_fn(std::index_sequence<Is...>) { return func(std::get<Is>(std::forward<TupleType>(params))...); }
	};

	template<typename F, typename ...Args>
	static auto make_func_wrapper(F&& f, Args&&... args) { return func_wrapper<F, Args...>(std::forward<F>(f), std::forward<Args>(args)...); }

	template<typename F, typename ...Args>
	static typename func_wrapper<F, Args...>::BaseType* make_func_wrapper_pointer(F&& f, Args&&... args) { return new func_wrapper<F, Args...>(std::forward<F>(f), std::forward<Args>(args)...); }

	enum task_status
	{
		created,
		running,
		completed,
		canceled,
		faulted,
	};

	class task_cancelled : public std::exception {
	public:
		task_cancelled() = default;

		const char* what() const override {
			return "a task was cancelled";
		}
	};
	
	class aggregate_exception : public std::exception {
		template<typename T>
		friend class dispatch_block;
	private:
		std::vector<std::exception> inner_exceptions;

	public:
		auto begin() { return inner_exceptions.begin(); }
		auto begin() const { return inner_exceptions.begin(); }
		auto end() { return inner_exceptions.end(); }
		auto end() const { return inner_exceptions.end(); }

		size_t size() const { return inner_exceptions.size(); }

		aggregate_exception() = default;

		const char* what() const override {
			return "aggregate exception";
		}

		void add_exception(const std::exception& e) {
			inner_exceptions.push_back(e);
		}

		void add_exception(const aggregate_exception& es) {
			for (const auto& e : es) {
				inner_exceptions.push_back(e);
			}
		}
	};

	struct cancel_block {
		std::atomic<bool> canceled;
		cancel_block() : canceled(false) {}
		void cancel() { canceled.exchange(true); }
	};

	class cancellation_token {
	private:
		std::shared_ptr<cancel_block> block;

	public:
		cancellation_token() = default;

		cancellation_token(const std::shared_ptr<cancel_block>& blockIn) : block(blockIn) {}

		bool is_cancellation_requested() const { if (block == nullptr) return false; return block->canceled; }

		void throw_if_cancellation_requested() const { if (block != nullptr && block->canceled) throw task_cancelled(); }
	};

	class cancellation_token_source {
		friend class cancellation_token;
	private:
		std::shared_ptr<cancel_block> impl;

	public:
		cancellation_token_source() : impl(std::make_shared<cancel_block>()) {}

		cancellation_token token() { return { impl }; }

		std::shared_ptr<cancel_block> _block() const { return impl; }

		void cancel() { impl->cancel(); }
	};

	template<typename T>
	class task_awaiter;

	template<typename T>
	class task_base;

	template<typename T>
	class task;

	struct task_t {
		virtual void wait() = 0;

		virtual void dispatch() = 0;
	};

	class child_disaptch_block {
	private:
		struct one_time_event {
			std::promise<void> p;
			std::future<void> f;

			one_time_event() : f(p.get_future()) {}

			void set() { p.set_value(); }
			void wait() { f.wait(); }
		};
		bool dispatched;
		std::mutex mtx;
		std::vector<std::unique_ptr<task_t>> childs;
		std::unique_ptr<one_time_event> child_would_dispatch;

	protected:
		bool is_self_child;

		child_disaptch_block(bool child)
			:
			is_self_child(child),
			dispatched(false)
		{
			if (is_self_child) {
				child_would_dispatch = std::make_unique<one_time_event>();
			}
		}

		void wait_if_is_self_child() {
			if (child_would_dispatch) {
				child_would_dispatch->wait();
			}
		}

		void dispatched_if_is_self_child() {
			if (child_would_dispatch) {
				child_would_dispatch->set();
			}
		}

		template<typename T>
		bool add_child(const task<T>& child) {
			std::unique_ptr<task_t> task_pointer(new task<T>(child));
			std::lock_guard<std::mutex> lk(mtx);
			if (!dispatched) {
				childs.push_back(std::move(task_pointer));
				return true;
			}
			else {
				return false;
			}
		}

		std::vector<std::unique_ptr<task_t>> mark_dispatch() {
			std::lock_guard<std::mutex> lk(mtx);
			if (dispatched) {
				throw std::logic_error("child tasks already dispatched");
			}
			dispatched = true;

			return std::move(childs);
		}
	};

	template<typename T>
	class dispatch_block : public child_disaptch_block
	{
		friend class task_base<T>;
		friend class task<T>;
		friend class task_awaiter<T>;
	private:
		task_status	status;
		cancellation_token cancel_token;
		std::shared_ptr<aggregate_exception> exception_ptr;

		std::atomic<bool> dispatch_once;
		std::future<void> dispatch_token;

		std::promise<T> result_token_setter;
		std::future<T> result_token;

	public:
		dispatch_block(const cancellation_token& token, const bool& child_in) 
			: 
			child_disaptch_block(child_in),
			status(created),
			cancel_token(token),
			dispatch_once(false),
			result_token_setter{},
			result_token(result_token_setter.get_future()),
			exception_ptr(nullptr)
		{}

		dispatch_block(const bool& child_in) : dispatch_block(cancellation_token{}, child_in) {}
		dispatch_block() : dispatch_block(false) {}

		void add_exception(const std::exception& e) {
			if (exception_ptr == nullptr) {
				exception_ptr = std::make_shared<aggregate_exception>();
			}
			exception_ptr->add_exception(e);
		}

		void add_exception(const aggregate_exception& e) {
			if (exception_ptr == nullptr) {
				exception_ptr = std::make_shared<aggregate_exception>();
			}
			exception_ptr->add_exception(e);
		}

		bool has_exception() const {
			if (exception_ptr != nullptr) {
				return exception_ptr->size() != 0;
			}
			return false;
		}

		const aggregate_exception& exception() const {
			if (exception_ptr == nullptr) {
				const static aggregate_exception empty;
				return empty;
			}

			return *exception_ptr;
		}

		bool is_child() const { return is_self_child; }

		bool is_canceled() const { return cancel_token.is_cancellation_requested(); }

		bool is_dispatchable() { 
			bool expected = false; 
			return dispatch_once.compare_exchange_strong(expected, true); 
		}

		void set_dispatched(std::future<void>&& dispatch_token_in) {
			status = running;
			dispatch_token = std::move(dispatch_token_in);
			dispatched_if_is_self_child();
		}
	};

	template<typename T>
	class task_base : public task_t
	{
	protected:
		std::shared_ptr<callable_t<T>> callable;
		std::shared_ptr<dispatch_block<T>> signal;

	protected:
		task_base(callable_t<T>* const& callableIn, bool child = false)
			:
			callable(callableIn),
			signal(std::make_shared<dispatch_block<T>>(child))
		{
		}

		task_base(callable_t<T>* const& callableIn, const cancellation_token& token, bool child = false)
			:
			callable(callableIn),
			signal(std::make_shared<dispatch_block<T>>(token, child))
		{
		}

		task_base(const task_base& rhs) {
			callable = rhs.callable;
			signal = rhs.signal;
		}

		task_base& operator=(const task_base& rhs) {
			callable = rhs.callable;
			signal = rhs.signal;
			return *this;
		}

		task_base(task_base&& rhs) noexcept {
			callable = std::move(rhs.callable);
			signal = std::move(rhs.signal);
		}

		task_base& operator=(task_base&& rhs) noexcept {
			callable = std::move(rhs.callable);
			signal = std::move(rhs.signal);
			return *this;
		}

		template<typename T>
		bool add_child(const task<T>& child) {
			return signal->add_child(child);
		}

		void dispatch_child() {
			auto childs = signal->mark_dispatch();
			for (auto& child : childs) {
				child->dispatch();
			}
		}

		void throw_if_child_task() {
			if (signal->is_child()) {
				throw std::logic_error("start child task");
			}
		}

	public:
		const aggregate_exception& exception() const {
			return signal->exception();
		}

		virtual void operator()() = 0;

		virtual void wait() override {
			signal->wait_if_is_self_child();
			signal->dispatch_token.wait();
		}

		bool is_canceled() const { return signal->status == canceled; }

		bool is_completed() const { return signal->status > running; }

		bool is_faulted() const { return signal->status == faulted; }

		bool is_completed_sucessfully() const { return signal->status == completed; }

		task_status get_status() const { return signal->status; }

		task_awaiter<T> get_awaiter() const { return { signal }; }
	};

	template<typename T>
	class task : public task_base<T>
	{
	public:
		task(callable_t<T>* const& callableIn, bool child = false) : task_base<T>(callableIn, child)
		{}

		task(callable_t<T>* const& callableIn, const cancellation_token& token, bool child = false) : task_base<T>(callableIn, token, child)
		{}

		virtual void operator()() override {
			auto& signal_obj = task_base<T>::signal;
			try {
				if (signal_obj->is_canceled()) {
					throw task_cancelled();
				}
				else {
					T&& value = (*task_base<T>::callable)();
					if (signal_obj->is_canceled()) {
						throw task_cancelled();
					}

					signal_obj->status = completed;
					signal_obj->result_token_setter.set_value(std::forward<T>(value));
				}
			}
			catch (const task_cancelled& e) {
				signal_obj->add_exception(e);
				signal_obj->status = canceled;
				signal_obj->result_token_setter.set_exception(std::current_exception());
			}
			catch (const aggregate_exception& e) {
				signal_obj->add_exception(e);
				signal_obj->status = faulted;
				signal_obj->result_token_setter.set_exception(std::current_exception());
			}
			catch (const std::exception& e) {
				signal_obj->add_exception(e);
				signal_obj->status = faulted;
				signal_obj->result_token_setter.set_exception(std::current_exception());
			}

			task_base<T>::dispatch_child();
		}

		virtual void dispatch() override {
			if (task_base<T>::signal->is_dispatchable()) {
				task_base<T>::signal->set_dispatched(std::async(std::launch::async, [task_obj = *this]() mutable { (task_obj)(); }));
			}
			else {
				throw std::exception("task is already started");
			}
		}

		void start() {
			task_base<T>::throw_if_child_task();
			dispatch();
		}

		template<typename F, typename R = std::decay_t<typename function_traits<std::decay_t<F>>::ReturnType>,
			typename = std::enable_if_t<std::is_same_v<typename decay_tuple_type<typename function_traits<std::decay_t<F>>::FArgsType>::type, std::tuple<task<T>>>>>
		task<R> then(F&& fIn);

		T get() { return task_base<T>::signal->result_token.get(); }
	};

	template<>
	class task<void> : public task_base<void>
	{
	public:
		task(callable_t<void>* const& callableIn, bool child = false) : task_base<void>(callableIn, child)
		{}

		task(callable_t<void>* const& callableIn, const cancellation_token& token, bool child = false) : task_base<void>(callableIn, token, child)
		{}

		virtual void operator()() override {
			auto& signal_obj = task_base<void>::signal;
			try {
				if (signal_obj->is_canceled()) {
					throw task_cancelled();
				}
				else {
					(*task_base<void>::callable)();
					if (signal_obj->is_canceled()) {
						throw task_cancelled();
					}

					signal_obj->status = completed;
					signal_obj->result_token_setter.set_value();
				}
			}
			catch (const task_cancelled& e) {
				signal_obj->add_exception(e);
				signal_obj->status = canceled;
				signal_obj->result_token_setter.set_exception(std::current_exception());
			}
			catch (const aggregate_exception& e) {
				signal_obj->add_exception(e);
				signal_obj->status = faulted;
				signal_obj->result_token_setter.set_exception(std::current_exception());
			}
			catch (const std::exception& e) {
				signal_obj->add_exception(e);
				signal_obj->status = faulted;
				signal_obj->result_token_setter.set_exception(std::current_exception());
			}

			task_base<void>::dispatch_child();
		}

		virtual void dispatch() override {
			if (task_base<void>::signal->is_dispatchable()) {
				task_base<void>::signal->set_dispatched(std::async(std::launch::async, [task_obj = *this]() mutable { (task_obj)(); }));
			} else {
				throw std::exception("task is already started");
			}
		}

		void start() {
			throw_if_child_task();
			dispatch();
		}

		template<typename F, typename R = std::decay_t<typename function_traits<std::decay_t<F>>::ReturnType>,
			typename = std::enable_if_t<std::is_same_v<typename decay_tuple_type<typename function_traits<std::decay_t<F>>::FArgsType>::type, std::tuple<task<void>>>>>
		task<R> then(F&& fIn);

		void get() { task_base<void>::signal->result_token.get(); }
	};

	template<typename T>
	class task_awaiter {
	private:
		std::shared_ptr<dispatch_block<T>> signal;

	public:
		task_awaiter(const std::shared_ptr<dispatch_block<T>>& signalIn) : signal(signalIn) {}

		bool is_completed() { return signal->status > running; }

		T get_result() { return signal->result_token.get(); }
	};

	template<typename F, typename ...Args>
	static inline auto make_task(F&& f, Args&&... args)
	{
		return task<typename func_wrapper<F, Args...>::ReturnType>(make_func_wrapper_pointer(std::forward<F>(f), std::forward<Args>(args)...));
	}

	template<typename F, typename ...Args>
	static inline auto make_task_with_cancellation_token(const cancellation_token& token, F&& f, Args&&... args)
	{
		return task<typename func_wrapper<F, Args...>::ReturnType>(make_func_wrapper_pointer(std::forward<F>(f), std::forward<Args>(args)...), token);
	}

	template<typename F, typename ...Args>
	static inline auto run_async(F&& f, Args&&... args)
	{
		auto task_source = make_task(std::forward<F>(f), std::forward<Args>(args)...);
		auto fire_and_forget = std::async(std::launch::async, [](auto&& task_obj) { task_obj.dispatch(); }, task_source);

		return task_source;
	}

	template<typename T> template<typename F, typename R, typename>
	task<R> task<T>::then(F&& fIn)
	{
		auto entangled = [f = std::forward<F>(fIn), task_obj = *this]() mutable {
			task_obj.wait();
			return f(task_obj);
		};

		auto child_task = task<R>(make_func_wrapper_pointer(entangled), true);
		if (!task_base<T>::add_child(child_task))
		{
			child_task.dispatch();
		}

		return child_task;
	}

	template<typename F, typename R, typename>
	task<R> task<void>::then(F&& fIn)
	{
		auto entangled = [f = std::forward<F>(fIn), task_obj = *this]() mutable {
			task_obj.wait();
			return f(task_obj);
		};

		auto child_task = task<R>(make_func_wrapper_pointer(entangled), true);
		if (!task_base<void>::add_child(child_task))
		{
			child_task.dispatch();
		}

		return child_task;
	}
}