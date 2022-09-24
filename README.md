# CppTask
### C++ implemented Task Base Programming
- C++ task class like C# (might be considered as other languages' task class)

# Examples (C++ vs C#)
### Create, Start and Wait A Task
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
try
{
    var r4 = t4.Result;
}
catch (Exception ex)
{
    Console.WriteLine($"{ex.GetType().Name}");
}
```
### Launch, Continue, and Cancel A Task
1. launch task
```cpp
auto t1 = run_async([]() {
	std::this_thread::sleep_for(std::chrono::seconds(2));
	throw std::exception("noop");
});
```
```csharp
var t1 = Task.Run(async () =>
{
    await Task.Delay(2000);
    throw new Exception("noop");
});
```
2. continue with the other task
```cpp
auto t2 = t1.then([](task<void>& t) {
	if (t.is_faulted())
	{
		cout << "previous task was faulted by exception" << endl;
	}
	else {
		cout << "not faulted by exception" << endl;
	}
});
```
```csharp
var t2 = t1.ContinueWith((t) =>
{
    if (t.IsFaulted)
    {
        Console.WriteLine("previous task was faulted by exception");
    }
    else
    {
        Console.WriteLine("not faulted by exception");
    }
});
```
3. cancel a task
```cpp
auto cancel_source = cancellation_token_source{};
	auto cancel_token = cancel_source.token();
	auto t3 = t2.then([cancel_token](task<void>& t) {
		std::this_thread::sleep_for(std::chrono::seconds(2));
		cancel_token.throw_if_cancellation_requested();
	});
cancel_source.cancel();
```
```csharp
var cancel_source = new CancellationTokenSource();
var cancel_token = cancel_source.Token;
var t3 = t2.ContinueWith(t =>
{
    Thread.Sleep(2000);// not good example
    cancel_token.ThrowIfCancellationRequested();
});
cancel_source.Cancel();
```
4. is task canceled?
```cpp
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
```
```csharp
 var t4 = t3.ContinueWith(t =>
{
    if (t.IsCanceled)
    {
        Console.WriteLine("previous task was canceled");
    }
    else
    {
        Console.WriteLine("not canceled");
    }
});

t4.Wait();
```