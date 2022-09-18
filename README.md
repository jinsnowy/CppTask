# CppTask

## C++ Task Base Programming

make C++ task class like C# or other languages' task class

'''
auto& worker = async_worker::get();
auto awaiter1 = worker.push_task([arr]() -> int { cout << "first task" << endl; return arr[rand()%10]; });
auto awaiter2 = awaiter1.continue_with([](int r) -> int { cout << "second task num choose = " << r << endl;  return r; });
cout << "result : " << awaiter2.get() << endl;
'''