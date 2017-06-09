//#define Csz_test
#define Csz_O1
#include <iostream>
#include "ThreadPool.hpp"
#include "ServiceUdp.h"

int main()
{
	{
	//fd_set listen_list;
	//int max_desc= STDIN_FILENO;
	//FD_ZERO(&listen_list);
	//FD_SET(STDIN_FILENO,&listen_list);

	std::shared_ptr<Csz_ThreadPool::ThreadPool> pool(std::make_shared<Csz_ThreadPool::ThreadPool>(30,90)); //10个线程，30个任务队列
	AdvanceUdp one(54321,6); //端口，udp环形链表
	std::thread t1(std::mem_fun(&AdvanceUdp::ServiceUdpGo),&one,pool);
	std::cout<<"start\n";
	
	//if (-1== select(max_desc+1,&listen_list,nullptr,nullptr,nullptr))
//	{
//		;
//	}
//	if (FD_ISSET(STDIN_FILENO,&listen_list))
//	{
	std::cin.get();
	one.End();
	std::cin.get();
	pool->End();
//	}
	t1.join();
	std::cout<<"end\n";
	}
	//Data data(1,2.0,false);
	//Csz_ThreadPool::ThreadPool pool(6,30);
	//std::cout<<"main_begin\n";
	//pool.AddTask(&func,Data(1,2.0,true));
	//std::cin.get();
	//pool.End();
	//std::cout<<"main_end\n";
	/*
	for (int i= 0; i< 30; i++)
	{
		pool.AddTask([]{std::cout<<"hello"<<"\n";});
	}
	pool.Stop();
	std::cout<<"stop:";
	std::cin.get();
	pool.Restart();
	std::cin.get();
	pool.End();
	*/
	return 0;
}
