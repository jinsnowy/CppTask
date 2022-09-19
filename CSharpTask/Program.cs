using System;
using System.Threading;
using System.Threading.Tasks;

namespace CSharpTask
{
    internal class Program
    {
        static void Test1()
        {
            var t1 = new Task(() => Console.WriteLine("hello world"));
            t1.Start();
            t1.Wait();

            // throw InvalidOperationException 'the task already started'
            try {
                t1.Start(); 
            } catch (InvalidOperationException ex) {
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

            Console.WriteLine($"third result : {r3}" );

            bool always = true;
            var t4 = Task.Run(() => { Thread.Sleep(1000); if (always) throw new InvalidProgramException("noop"); return 10; });

            try
            {
                //t4.Wait();
                var r4 = t4.Result;
            }
            catch (Exception ex) {
                // Aggregate Exception
                Console.WriteLine($"{ex.GetType().Name}"); 
            } 
        }

        static void Main(string[] args)
        {
            Test1();
        }
    }
}
