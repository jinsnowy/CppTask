using System;
using System.Threading;
using System.Threading.Tasks;

namespace CSharpTask
{
    internal class Program
    {
        static void Main(string[] args)
        {
            Test1();
            Test2();
        }

        static void Test1()
        {
            var t1 = new Task(() => Console.WriteLine("hello world"));
            t1.Start();
            t1.Wait();

            // throw InvalidOperationException 'the task already started'
            try
            {
                t1.Start();
            }
            catch (InvalidOperationException ex)
            {
                Console.WriteLine($"{ex.GetType().Name}");
            }

            int a = 1, b = 2;
            var t2 = new Task<int>(() => { return a + b; });
            t2.Start();

            var r2 = t2.Result;

            Console.WriteLine($"second result : {r2}");

            var t3 = Task.Run(() => "hello world");
            var awaiter = t3.GetAwaiter();
            var r3 = awaiter.GetResult();

            Console.WriteLine($"third result : {r3}");

            bool always = true;
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
        }

        static void Test2()
        {
            var t1 = Task.Run(async () =>
            {
                await Task.Delay(1000);
                Thread.Sleep(2000); 
                throw new Exception("noop");
            });

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

            var cancel_source = new CancellationTokenSource();
            var cancel_token = cancel_source.Token;
            var t3 = t2.ContinueWith(t =>
            {
                Thread.Sleep(2000);// not good example
                cancel_token.ThrowIfCancellationRequested();
            });
            cancel_source.Cancel();

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
        }
    }
}
