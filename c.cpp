//#define Csz_test
#define Csz_O1
#include <iostream>
#include "ThreadPool.hpp"
#include "ServiceUdp.h"

int main()
{
	{
	std::shared_ptr<Csz_ThreadPool::ThreadPool> pool(std::make_shared<Csz_ThreadPool::ThreadPool>(30,90)); //30个线程，90个任务队列
	AdvanceUdp one(54321,6); //端口，udp环形链表
	std::thread t1(std::mem_fun(&AdvanceUdp::ServiceUdpGo),&one,pool);
	std::cout<<"start\n";
	std::cin.get();
	one.End();
	std::cin.get();
	pool->End();
	t1.join();
	std::cout<<"end\n";
	}
	return 0;
}
