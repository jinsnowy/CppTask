#include "task.h"
#include <set>
#include <chrono>

using namespace std;
using namespace tpl;
using namespace chrono_literals;

int test_num = 512;

void test1();

int main()
{
	test1();

	return 0;
}

void test1()
{
	auto t1 = make_task([]() { cout << "hello world" << endl; });
	t1.start();
	t1.wait();

	try {
		t1.start();
	}
	catch (std::exception e) {
		cout << e.what() << endl;
	}

	int a = 1, b = 2;
	auto t2 = make_task([](int a, int b) { return a + b; }, a, b);
	// auto t2 = make_task([a, b]() { return a + b; });
	t2.start();

	auto r2 = t2.result();
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
		auto r4 = t4.result();
	}
	catch (const std::exception& e) {
		cout << e.what() << endl;
	}
}