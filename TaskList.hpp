#include <mutex> //mutex
#include <condition_variable> //condition_variable
#include <utility> //forward
#include <list> //std::list

#include "Any.hpp"

#ifndef Csz_TaskList_H
#define Csz_TaskList_H

#define SIZE 30   //默认30个容量任务

//#define Csz_test

	//Func 方法类
	template<typename Func>
	class TaskList
	{
		private:
			bool stop_flag;  //标记是否停止
			uint8_t size_max; //最大任务数
			std::mutex task_mutex; //锁
			std::condition_variable is_full;  //为满时等待
			std::condition_variable is_empty;  //为空时等待
			std::list<std::pair<Func,Any>> task_list; //以Any类擦除了函数参数的不同
		private:
			//这里用到引用未定义，若传进来是右值则为右值引用，其他则为左值
			//const Task && T_task 加了const 则不是引用未定义，只是个右值引用参数
			template <typename Task,typename Parameter>
			void Add(Task && T_task,Parameter &&T_any)
			{
				std::unique_lock<std::mutex> guard(task_mutex); //上锁
				//若不能执行则释放锁进入等待，加了第二参数相当
				//while (!predicate())
				//		is_full.wait();
				is_full.wait(guard,[this]{return stop_flag || !IsFull();}); 
				if (true== stop_flag)
				{
					//任务是被停止了而不是结束，有可能继续，为了保证任务不丢失
					//再次检查是否为满，不为满则加入任务
					if (!IsFull())
						task_list.emplace_back(std::forward<Task>(T_task),std::forward<Parameter>(T_any));
					return ;
				}
#if defined(Csz_test)
				std::cout<<"TaskList::Add::emplace_back\n";
#endif
				//就地构造，避免了一次没必要的移动拷贝或者赋值拷贝
				task_list.emplace_back(std::forward<Task>(T_task),std::forward<Parameter>(T_any));
				is_empty.notify_one(); //此时不为空，所以唤起因为空而等待的线程
			}
		public:
			TaskList(const uint8_t &T_size= SIZE) noexcept : stop_flag(false),size_max(T_size){}
			~TaskList()noexcept 
			{
#if defined(Csz_test)
				std::cout<<"TaskList::destructor\n";
#endif
				task_list.clear();
			}
			TaskList(const TaskList &T_other)=delete;
			TaskList(TaskList &&T_other)=delete;
			TaskList& operator=(const TaskList &T_other)=delete;
			TaskList& operator=(TaskList &&T_other)=delete;
			//任务添加为左值
			void Put(const Func &T_task,const Any &T_any)
			{
#if defined(Csz_test)
				std::cout<<"TaskList::Put()copy\n";
#endif
				Add(T_task,T_any);
			}
			//任务添加为右值
			//利用完美转发保持参数以右值形式传进
			void Put(Func &&T_task,Any &&T_any)
			{
#if defined(Csz_test)
				std::cout<<"TaskList::Put()move\n";
#endif
				Add(std::forward<Func>(T_task),std::forward<Any>(T_any)); //forward<int>(value) 为右值
			}
			//不返回值，利用参数加引用，假设在没有返回值优化的情况下，那么将比有返回值
			//的少了一个constructor以及一个copy
			void Take(Func &T_task,Any &T_any)
			{
				std::unique_lock<std::mutex> guard(task_mutex);
				//若不能执行则释放锁进入等待，加了第二个参数相当于
				//while(!predicate())
				//		is_empty.wait();
				is_empty.wait(guard,[this]{return stop_flag || !IsEmpty();});
				if (true== stop_flag)
				{
					//T_task=[]{}; //先判断后运行
					return ;
				}
#if defined(Csz_test)
				std::cout<<"TaskList::Take()\n";
#endif
				auto task= std::move(task_list.front());
				task_list.pop_front();
				T_task= std::move(task.first);
				T_any= std::move(task.second);
				is_full.notify_one();  //此时不为满，所以唤醒因为满而进行等待的线程
			}
			void Stop() noexcept
			{
#if defined(Csz_test)
				std::cout<<"TaskList::Stop()\n";
#endif
				{
					//设为局部锁，若在加锁的情况下唤醒其他等待线程，
					//则其他线程必须等待mutex的释放
					std::unique_lock<std::mutex> guard(task_mutex);
					stop_flag= true; 
				}
				is_full.notify_all();
				is_empty.notify_all();
			}
			void Restart() noexcept
			{
#if defined(Csz_test)
				std::cout<<"TaskList::Restart()\n";
#endif
				std::lock_guard<std::mutex> guard(task_mutex);
				//size_max= T_size;
				stop_flag= false;
			}
		private:
			//在Add函数中调用到，而Add函数本身是在有锁的情况下进行
			//若在此函数再进行加锁着就会出现饿死的情况也就是死锁
			bool IsFull() const noexcept
			{
#if defined(Csz_test)
				std::cout<<"TaskList::IsFull()\n";
#endif
				return task_list.size()== size_max;
			}
			//同上 在take
			bool IsEmpty() const noexcept
			{
#if defined(Csz_test)
				std::cout<<"TaskList::IsEmpty()\n";
#endif
				return task_list.empty();
			}
		public:
			//若是在外调用，保证返回值是当前的准确值
			//加锁的情况下
			size_t Size() const noexcept
			{
#if defined(Csz_test)
				std::cout<<"TaskList::Size()\n";
#endif
				std::lock_guard<std::mutex> guard(task_mutex);
				return task_list.size();
			}
			void Clear()
			{
#if defined(Csz_test)
				std::cout<<"TaskList::Clear()\n";
#endif
				std::lock_guard<std::mutex> guard(task_mutex);
				task_list.clear();
			}
	};
#endif //Csz_TaskList_H
