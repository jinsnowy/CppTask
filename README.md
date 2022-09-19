# CppTask

## C++ Task Base Programming

make C++ task class like C# or other languages' task class

- example1
```cpp
auto t1 = make_task([]() { cout << "hello world" << endl; });
t1.start();
t1.wait();
```
```csharp
var t1 = new Task(() => Console.WriteLine("hello world"));
t1.Start();
t1.Wait();
```