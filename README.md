# CppTask

## C++ Task Base Programming

make C++ task class like C# or other languages' task class

- example
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

```cpp
int a = 1, b = 2;
auto t2 = make_task([](int a, int b) { return a + b; }, a, b);
t2.start();
```

```csharp
int a = 1, b = 2;
var t2 = new Task<int>(() => { return a + b; });
t2.Start();
```