# CppTask
### C++ implemented Task Base Programming
- C++ task class like C# (might be considered as other languages' task class)


### Create Task, Start and Wait The Task
1. start task and wait
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

2. task using captured parameters
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

3. awaiter class
```cpp
auto t3 = run_async([]() { return "hello world"; });
auto awaiter = t3.get_awaiter();
auto r3 = awaiter.get_result();
```

```csharp
var t3 = Task.Run(() => "hello world");
var awaiter = t3.GetAwaiter();
var r3 = awaiter.GetResult();
```

4. throw exception in task
```cpp
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
```csharp
var t4 = Task.Run(() => 
{ 
    Thread.Sleep(1000);
    if (always)
    {
        throw new InvalidProgramException("noop");
    }
                 
    return 10; 
});
```