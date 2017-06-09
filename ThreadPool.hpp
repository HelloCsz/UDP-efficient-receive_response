#include <forward_list> //forward_list单向链表
#include <thread> //thread
#include <memory> //unique_ptr
#include <functional> //function
#include <atomic> //atomic_bool 原子操作
#include <mutex> //mutex,once_flag,call_once
#include "TaskList.hpp"

#ifndef Csz_ThreadPool_H
#define Csz_ThreadPool_H

//#define Csz_test

//#if defined(Csz_test)
//#include <chrono>
//#endif

namespace Csz_ThreadPool
{
	class ThreadPool 
	{
		private:
			//stop_flag停止线程运行，Stop method and Restart method
			//停止后还是能继续往task_list中加入任务，而线程都处于waiting状态
			//因为Stop()锁住了stop_mutex,而线程们正不断'努力'获取这个锁调用
			//std::lock_guard<std::mutex> guard(stop_mutex) RAII机制，
			//而在Restart()释放了stop_mutex,线程依次获取此锁后,出了局部范围
			//又释放了此锁，以便其他线程获取此锁。
			//暂停与重新开始(不建议多线程调用，因为可能会发生死锁或者抛出异常)
			std::atomic_bool stop_flag;
			std::mutex stop_mutex;
			//std::once_flag 调用法std::call_once(promise_one,[]{})若是在调用
			//函数里发生了异常或者扔出了异常则下次调用是会再次运行
			//保证线程池的释放只进行一次(在多线程同时操作线程池ing)
			//std::atomic_bool 提供了原子操作
			std::once_flag promise_one;
			std::atomic_bool end_flag;
			//若直接把线程放进container里则进行拷贝，若放进线程指针则需要
			//注意在join时候必须释放内存，若使用智能指针则可以同时避免着两种情况
			//使用forward_list原因:只需要把线程指针从一端放进去而不需要从另一端取出来
			//而forward_list比list在空间利用率上更高
			using ThreadPtr= std::unique_ptr<std::thread>;
			std::forward_list<ThreadPtr> thread_list; //线程队列
			using Task= std::function<void(Any)>;
			TaskList<Task> task_list; //任务队列
		private:
			//初始化，创建线程并把线程move进去线程队列，方便结束线程池时
			//join线程
			void Start(const int & T_thread_num) noexcept
			{
#if defined(Csz_test)
				std::cout<<"Thread::Start()\n";
#endif
				for (int i= 0; i< T_thread_num; i++)
				{
					thread_list.push_front(std::move(ThreadPtr(new std::thread(&ThreadPool::ThreadRun,this))));
					//thread_list.emplace_front(&ThreadPool::ThreadRun,this); //重载make_unique
				}
			}
			//线程执行的函数
			void ThreadRun() noexcept
			{
				Task task;
				Any parameter;
				while (!end_flag)
				{
					task_list.Take(task,parameter);
					if (end_flag)
						return ;
#if defined(Csz_test)
					std::cout<<"ThreadPool::ThreadRun task_begin\n";
#endif
					task(std::move(parameter));
#if defined(Csz_test)
					std::cout<<"ThreadPool::ThreadRun task_end\n";
					//std::this_thread::sleep_for(std::chrono::seconds(1));
#endif
					if (stop_flag)
					{
						std::lock_guard<std::mutex> guard(stop_mutex);
					}
				}
			}
			//结束线程，等待线程执行完当前task,这里有个debug,也就是说
			//你的结束并不是立即结束而是必须等待线程把所领取的task执行完
			//最后清空线程队列以及任务队列
			void ThreadPoolEnd() noexcept
			{
#if defined(Csz_test)
				std::cout<<"ThreadPool::ThreadPoolEnd()\n";
#endif
				end_flag=true;
				task_list.Stop();
				for (auto &thread : thread_list)
					if (thread)
						thread->join();
				thread_list.clear();
				task_list.Clear();
			}
		public:
			//构造函数，在赋值参数的时候要严格按照申明顺序！！！
			//parameter1 线程队列最大容量，默认为系统核数
			//parameter2 任务队列最大容量，默认为 #define SIZE 30
			ThreadPool(int T_thread_num= std::thread::hardware_concurrency(),uint8_t T_size= SIZE)noexcept : stop_flag(false),end_flag(false),task_list(T_size)
			{
				Start(T_thread_num);
			}
			//当忘了结束线程池则有析构函数来执行
			~ThreadPool()
			{
#if defined(Csz_test)
				std::cout<<"ThreadPool::destructor\n";
#endif
				if (true== stop_flag)
					Restart();
				End();
			}
			//添加任务，右值
			//使用完美转发保证了参数类型原样
			void AddTask(Task &&T_task,Any &&T_parameter) noexcept
			{
#if defined(Csz_test)
				std::cout<<"ThreadPool::AddTask()move\n";
#endif
				task_list.Put(std::forward<Task>(T_task),std::forward<Any>(T_parameter));
			}
			//添加任务，除了右值
			void AddTask(const Task &T_task,const Any &&T_parameter) noexcept
			{
#if defined(Csz_test)
				std::cout<<"ThreadPool::AddTask()copy\n";
#endif
				task_list.Put(T_task,T_parameter);
			}
			//结束线程池
			//call_once保证只正常执行一次
			void End() noexcept
			{
#if defined(Csz_test)
				std::cout<<"ThreadPool::End()\n";
#endif
				std::call_once(promise_one,[this]{ThreadPoolEnd();});
			}
			//停止线程，但不停止任务添加
			void Stop() noexcept
			{
#if defined(Csz_test)
				std::cout<<"ThreadPool::Stop()\n";
#endif
				//多线程控制线程池
				if (true== stop_flag)
					return ;
				stop_mutex.lock();
				stop_flag= true;
			}
			//恢复线程，线程继续从任务队列中取任务执行
			void Restart() noexcept
			{
#if defined(Csz_test)
				std::cout<<"ThreadPool::Restart()\n";
#endif
				//Start(T_thread_num);
				//多线程控制线程池
				if (false== stop_flag)
					return ;
				stop_flag= false;
				stop_mutex.unlock();
			}
	};
}
#endif //Csz_ThreadPool_H
