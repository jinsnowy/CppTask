#include "task.h"
#include <set>
#include <chrono>

using namespace std;
using namespace cpptask;
using namespace chrono_literals;

int test_num = 512;

void test1();
void test2();
void test3();

int main()
{
	//test1();
	//test2();
	test3();

	return 0;
}

void test1()
{
	auto t1 = make_task([]() { cout << "hello world" << endl; throw std::exception("noop"); });
	t1.start();

	try {
		t1.wait();
	}
	catch (const std::exception& e) {
		cout << e.what() << endl;
	}

	int a = 1, b = 2;
	auto t2 = make_task([](int a, int b) { return a + b; }, a, b);
	// auto t2 = make_task([a, b]() { return a + b; });
	t2.start();

	auto r2 = t2.get();
	cout << "second result : " << r2 << endl;

	auto t3 = run_async([]() { return "hello world"; });
	auto awaiter = t3.get_awaiter();
	auto r3 = awaiter.get_result();

	cout << "third result : " << r3 << endl;

	bool always = true;
	auto t4 = run_async([always]() { 
		std::this_thread::sleep_for(std::chrono::seconds(1));
		if (always)
		{
			throw std::exception("noop");
		}
	
		return 10; 
	});

	try {
		auto r4 = t4.get();
	}
	catch (const std::exception& e) {
		cout << e.what() << endl;
	}
}

void test2()
{
	auto t1 = run_async([]() {
		std::this_thread::sleep_for(std::chrono::seconds(2));
		throw std::exception("noop");
	});

	auto t2 = t1.then([](task<void>& t) {
		if (t.is_faulted())
		{
			cout << "previous task was faulted by exception" << endl;
		}
		else {
			cout << "not faulted by exception" << endl;
		}
	});

	auto cancel_source = cancellation_token_source{};
	auto cancel_token = cancel_source.token();
	auto t3 = t2.then([cancel_token](task<void>& t) {
		std::this_thread::sleep_for(std::chrono::seconds(2));
		cancel_token.throw_if_cancellation_requested();
	});
	cancel_source.cancel();

	auto t4 = t3.then([](task<void>& t) {
		if (t.is_canceled())
		{
			cout << "previous task was canceled" << endl;
		}
		else {
			cout << "not canceled" << endl;
		}
	});

	t4.wait();
}

void test3()
{
	auto e1 = run_async([]() { cout << "first task starts" << endl; throw std::exception("my exception"); });
	auto v1 = e1.then([](task<void>& t) {
		if (t.is_faulted()) {
			const auto& ex = t.exception();
			for (const auto& e : ex) {
				cout << e.what() << endl;
			}
		}
	});

	v1.wait();
}