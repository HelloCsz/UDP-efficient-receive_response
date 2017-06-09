#include <sys/socket.h> //sendto,fromrecv
#include <netinet/in.h> //htons/htonl/ntohs/ntohl/ struct sockaddr_in
#include <arpa/inet.h> //inet_ntop
#include <unistd.h> //close
#include <cstring> //memset
#include <stdexcept> //std::out_of_range
#include <typeinfo> //std::bad_cast
#include <vector> //vector
#include <functional> //unary_function,std::mem_fun
#include <signal.h>
#include "ScanJson.hpp"  // std::shared_ptr<std::vector<uint32_t>> operator()(const char *file_name)

#ifndef Csz_ServiceUdp_H
#define Csz_ServiceUdp_H

#define UdpNum 4  //环形Udp链表个数
#define TIMEOUT 3 //超时
#define JSON_MV "Json/movie.txt"
#define JSON_OS "Json/os.txt"
#define JSON_SW "Json/soft.txt"
#define JSON_TV "Json/tv.txt"

//#define Csz_test

//基础udp类，创建了初始化sock，关闭sock，获取sock方法
//其一，AdvanceUdp类继承此类
//其二，在并发发送请求消息时需要多个udp发送消息，需要
//一个循环队列，而循环队列的成员就是此类
class CommonUdp
{
	protected:
	int sock;	//sock
	public:
	//初始化，AF_INET ipv4族，SOCK_DGRAM 数据流
	//IPPROTO_UDP UDP协议
	void Init()	
	{
		sock= socket(AF_INET,SOCK_DGRAM,IPPROTO_UDP);
		if (sock< 0)
		{
			throw std::out_of_range("socket() failed\n");
		}
	}
	//关闭sock
	void Close() const 
	{
		close(sock);
	}
	//获取sock
	int GetSock() const noexcept{return sock;}
	//析构 调用Close
	virtual ~CommonUdp()
	{
#if defined(Csz_test)
		std::cout<<"CommonUdp::destructor\n";
#endif
		Close();
	}
};

//Upd环形列表
//禁止了拷贝构造/移动构造/拷贝赋值/移动赋值为了避免内存泄露
class UdpAnnulus
{
	public:
		CommonUdp udp;
		UdpAnnulus *next;
		UdpAnnulus() noexcept
		{
#if defined(Csz_test)
			std::cout<<"UdpAnnulus constructor\n";
#endif
		}
		~UdpAnnulus() noexcept
		{
#if defined(Csz_test)
			std::cout<<"UdpAnnulus destructor\n";
#endif
		}
		UdpAnnulus(const UdpAnnulus &)=delete;
		UdpAnnulus(UdpAnnulus &&)=delete;
		UdpAnnulus& operator=(const UdpAnnulus &)=delete;
		UdpAnnulus& operator=(UdpAnnulus &&)=delete;
};

//广播方法类的参数，保存在Any类中
//data 接收请求码
//sock 发送UDP sock
//sentry 响应信息在文件中的位置标记(ifstream.seekg(sentry))
//client_addr 接收响应消息的对象
class Message
{
	public:
		char data[3];
		int sock;
		uint32_t sentry;
		sockaddr_in client_addr;
#if defined(Csz_test)
		~Message(){std::cout<<"Message::destructor\n";}
#endif
};

//对接收消息进行解析
//111× ×××× 前三位为类型 2^3=8种
//000× :0  001× :32  010× :64  011× 96
//100× :128  101× :160  110× :192  111× :224
//32:MV  64:TV  96:SW  128:OS
struct SelectType : public std::unary_function<char,uint8_t>
{
	uint8_t operator()(char &T_seek) const noexcept
	{
		return T_seek& 0xe0;
	}
};

//广播方法类
//参数为Any，在操作时进行转换
//对消息进行类型分类
struct UdpBroadCast : public std::unary_function<Any,void>
{
	void operator()(Any &&T_any)const noexcept
	{
		//broadcast
		//Any::AnyCast throw std::bad_cast
		Message value;
		try
		{
			value= T_any.AnyCast<Message>();
		}
		catch (std::bad_cast &T_mess)
		{
#if !defined(Csz_O1)
			std::cerr<<"UdpBroadCast::operator(): "<<T_mess.what()<<"\n";
#endif
			return ;
		}
#if defined(Csz_test)
		std::cout<<"UdpBroadCast::operator() request:"<<short(value.data[0])<<"\n";
#endif
		switch (SelectType()(value.data[0]))
		{
			case 32:
				SendTo(JSON_MV,value.sentry,value.sock,value.client_addr);
				break;
			case 64:
				SendTo(JSON_TV,value.sentry,value.sock,value.client_addr);
				break;
			case 96:
				SendTo(JSON_SW,value.sentry,value.sock,value.client_addr);
				break;
			case 128:
				SendTo(JSON_OS,value.sentry,value.sock,value.client_addr);
				break;
			default:
				//0,224,160,192
				return ;
		}
	}
	//实际广播方法
	//若T_sentry== -1 uint32_t -1对应最大值(请求页数大于最大值)
	//若T_sentry== -2 不合法请求码
	//则不做任何处理，请求页数超过了数据库所拥有的页数
	//文件打不开可能是文件名不对，或者句柄数量已到上限
	//std::string 也是不断动态申请空间，当当前空间不足时
	//将再申请一块更大的空间(通常是以前空间×2)，再释放
	//之前的空间，所以为了避免返回整合空间一开始就将空间
	//设置位较大，使用temmp.reserve();
	//若后面JSON是动态改变则可以在过后使用temp.shrink_to_fit()释放多余空间
	void SendTo(const char * T_file_name,const uint32_t &T_sentry,const int &T_sock,const sockaddr_in &T_client_addr) const noexcept
	{
		if (uint32_t(-2)== T_sentry)
		{
#if !defined(Csz_O1)
			char client_addr[16]={0};
			inet_ntop(AF_INET,&T_client_addr.sin_addr.s_addr,client_addr,sizeof(client_addr));
			std::cerr<<"ip "<<client_addr<<" port "<<ntohs(T_client_addr.sin_port)<<" RequestCode out of range\n";
#endif
			return ;
		}
		std::string temp;
		temp.reserve(32);
		std::ifstream handle(T_file_name);
		if (!handle.is_open())
		{
#if !defined(Csz_O1)
			std::cerr<<"file_name: "<<T_file_name<<" open failed\n";
#endif
			temp.assign("服务器数据错误");
		}
		else if (uint32_t(-1)== T_sentry)
		{
#if !defined(Csz_O1)
			char client_addr[16]={0};
			inet_ntop(AF_INET,&T_client_addr.sin_addr.s_addr,client_addr,sizeof(client_addr));
			std::cerr<<"ip "<<client_addr<<" port "<<ntohs(T_client_addr.sin_port)<<" request page> Max\n";
#endif
			temp.assign("请求页码大于最大值");
		}
		else
		{
		temp.reserve(1152);
		handle.seekg(T_sentry);
		std::getline(handle,temp);
		}
		//sendto 失败返回-1
		if (sendto(T_sock,temp.c_str(),temp.size(),0,(struct sockaddr*)&T_client_addr,sizeof(T_client_addr))< 0)
		{
#if !defined(Csz_O1)
			char client_addr[16]={0};
			inet_ntop(AF_INET,&T_client_addr.sin_addr.s_addr,client_addr,sizeof(client_addr));
			std::cerr<<T_sock<<": sendto "<<client_addr<<" port "<<ntohs(T_client_addr.sin_port)<<" failed\n";
#endif
			//记录
		}
	}
};

//继承了普通Udp类，有个sock，以及初始化/关闭/获取sock方法
//通过AdvanceUdp 实质是一个udp反复接收其他udp发送过来的请求信息
//然后进行分类处理并派送给其他udp去进行发送
//end_flag 结束标记，线程不断跑接收请求函数，是否结束则跟着次标记
//once_flag 保证关闭socket和释放环形链表只执行一次
//mess 将任务扔进线程池需要方法类以及其参数，而mess就是其参数
//RecordPtr 记录了json文件每条记录在文件中的位置，因为UDP是无状态
//所以需要认为记录'状态'
//调用End() 去设置end_flag为true，而ServiceUdpGo() 是while (!end_flag)
//调用 recvfrom 阻塞/等待,也就是说在没接收到任何信息时，是不会结束此线程
//循环。解决办法 1.设置超时 2.设置锁 condition_variable
class AdvanceUdp : public CommonUdp
{
	private:
		using RecordPtr= std::shared_ptr<std::vector<uint32_t>>;
	private:
		bool end_flag; //结束标记
		bool once_flag; //运行一次标记
		UdpAnnulus *guard; //udp环形链表 哨兵
		Message mess; //response消息
		RecordPtr mv_record; //智能指针shared_ptr
		RecordPtr os_record;
		RecordPtr sw_record;
		RecordPtr tv_record;
		int task_request;
	private:
		//初始化 RecordPtr
		void InitJsonFileRecord()
		{
#if defined(Csz_test)
			std::cout<<"AdvanceUdp::InitJsonFileRecord()\n";
#endif
			try
			{
				mv_record= ScanJson()(JSON_MV);
			}
			catch (const std::invalid_argument &message)
			{
				std::cerr<<message.what()<<"\n";
				exit(1);
			}
			try
			{
				os_record= ScanJson()(JSON_OS);
			}
			catch (const std::invalid_argument &message)
			{
				std::cerr<<message.what()<<"\n";
				exit(1);
			}
			try
			{
				sw_record= ScanJson()(JSON_SW);
			}
			catch (const std::invalid_argument &message)
			{
				std::cerr<<message.what()<<"\n";
				exit(1);
			}
			try
			{
				tv_record= ScanJson()(JSON_TV);
			}
			catch (const std::invalid_argument &message)
			{
				std::cerr<<message.what()<<"\n";
				exit(1);
			}
		}
		//创建环形链表
		//有两种方法 一种是不断的把新指针插在头的下一个(注释)，另一种是按顺下申请下去
		//每申请一个变量就为成员变量udp调用初始化函数
		void CreateUdpAnnulus(uint8_t &&T_num)
		{
#if defined(Csz_test)
			std::cout<<"AdvanceUdp::CreateUdpAnnulus()\n";
#endif
			guard= new UdpAnnulus();
			guard->next= nullptr;
			//异常
			try
			{
				guard->udp.Init();
			}
			catch (const std::out_of_range &T_mess)
			{
				std::cerr<<T_mess.what();
				delete guard;
				exit(1);
			}
			//UdpAnnulus *flag;
			UdpAnnulus *temp= guard;
			while (T_num> 1)
			{
				//UdpAnnulus *temp= new UdpAnnulus();
				//temp->next= guard->next;
				//guard->next= temp;
				temp->next= new UdpAnnulus();
				temp= temp->next;
				try
				{
					//guard->next->udp.Init();
					temp->udp.Init();
				}
				catch (const std::out_of_range &T_mess)
				{
					temp->next= nullptr;
					std::cerr<<T_mess.what();
					ExceptionUdpAnnulus();
					exit(1);
				}
				//if (nullptr== temp->next)
				//	flag= temp;
				--T_num;
			}
			//flag->next= guard;
			temp->next= guard; //尾的下一个是头
		}
		//销毁环形链表
		//先把头的下一个赋值给临时变量，再把头下一个置空
		//这时候就是个单向链表了，从临时变量开始进行删除
		void DestroyUdpAnnulus()
		{
#if defined(Csz_test)
			std::cout<<"AdvanceUdp::DestroyUdpAnnulus()\n";
#endif
			if (nullptr== guard)
				return ;
			UdpAnnulus *temp= guard->next;
			guard->next= nullptr;
			while (nullptr!= temp)
			{
				UdpAnnulus *dele= temp;
				temp= temp->next;
				delete dele;
			}
			guard= nullptr;
		}
		//环形链表初始化udp时抛出异常处理机制
		//在初始化环形链表时头是不变的
		void ExceptionUdpAnnulus()
		{
#if defined(Csz_test)
			std::cout<<"AdvanceUdp::ExceptionUdpAnnulus()\n";
#endif
			while (nullptr!=guard)
			{
				UdpAnnulus *dele= guard;
				guard= guard->next;
				delete dele;
			}
		}
		//获取环形链表中udp socket
		//然后guard移动到下一个
		int GetBroadCastSock()
		{
			int value= guard->udp.GetSock();
			guard= guard->next;
#if defined(Csz_test)
			std::cout<<"AdvanceUdp::GetBroadCastSock() sock:"<<value<<"\n";
#endif
			return value;
		}
		//判断是否是合法用户
		//第二个字符是否为空'\0'
		bool Check()
		{
			task_request++;
			std::cerr<<"task_request: "<<task_request<<"\n";
			if (mess.data[1]== 0)
				return true;
			else
				return false;
		}
		//返回所请求页数所在的位置(文件中的位置)
		//×××1 1111 后五位表示页数 2^5=64页
		//111× ×××× 前三位为类型 2^3=8种
		//000× :0  001× :32  010× :64  011× 96
		//100× :128  101× :160  110× :192  111× :224
		//32:MV  64:TV  96:SW  128:OS
		uint32_t RetNum(char &T_seek)
		{
#if defined(Csz_test)
			std::cout<<"AdvanceUdp::RetNum()\n";
#endif
			switch (SelectType()(T_seek))
			{
				case 32:
					if ((T_seek& 0x1f)>= mv_record->size())
						return -1;
					return (*mv_record)[T_seek& 0x1f];
				case 64:
					if ((T_seek& 0x1f)>= tv_record->size())
						return -1;
					return (*tv_record)[T_seek& 0x1f];
				case 96:
					if ((T_seek& 0x1f)>= sw_record->size())
						return -1;
					return (*sw_record)[T_seek& 0x1f];
				case 128:
					if ((T_seek& 0x1f)>= os_record->size())
						return -1;
					return (*os_record)[T_seek& 0x1f];
				default:
					//0,224,160,192 不合法请求码
					return -2;
			}
		}
	public:
		//构造函数，参数为udp环形链表的个数
		//Bind方法 扔出异常std::out_of_range
		//对T_num进行检测，不能是负数
		AdvanceUdp(uint16_t &&T_port,uint8_t &&T_num= UdpNum) : end_flag(false),once_flag(false),guard(nullptr),task_request(0)
		{
			try
			{
				Bind(std::forward<uint16_t>(T_port));
			}
			catch (const std::out_of_range &T_mess)
			{
				std::cerr<<T_mess.what();
				exit(1);
			}
			//T_num= T_num> 0? :T_num : 1;
			CreateUdpAnnulus(std::forward<uint8_t>(T_num));
			InitJsonFileRecord();
		}
		//析构函数，若已经手动释放则不做任何操作
		~AdvanceUdp()
		{
#if defined(Csz_test)
			std::cout<<"AdvanceUdp::destructor\n";
#endif
			if (!once_flag)
			{
				Free();
			}
		}
		AdvanceUdp(const AdvanceUdp &)=delete;
		AdvanceUdp(AdvanceUdp &&)=delete;
		AdvanceUdp& operator=(const AdvanceUdp &)=delete;
		AdvanceUdp& operator=(AdvanceUdp &&)=delete;
		//关闭udp socket(用于接收消息),释放环形链表
		//设置操作标记符为true
		void Free()
		{
			if (once_flag)
				return ;
#if defined(Csz_test)
			std::cout<<"AdvanceUdp::Free()\n";
#endif
			Close();
			DestroyUdpAnnulus();
			once_flag= true;
		}
		//将sock与地址/端口进行绑定
		//调用基类进行sock初始化
		//struct sockaddr_in ipv4地址结构体
		void Bind(uint16_t &&T_port)
		{
			try
			{
				Init();
			}
			catch (const std::out_of_range &T_mess)
			{
				std::cerr<<T_mess.what();
				exit(0);//处理机制
			}
			struct sockaddr_in server_address;
			memset(&server_address,0,sizeof(server_address)); //置零
			server_address.sin_family= AF_INET; //ipv4地址族
			server_address.sin_addr.s_addr= htonl(INADDR_ANY); //任何ip地址，转网络字节
			server_address.sin_port= htons(T_port); //端口号，转网络字节
			if (bind(sock,(struct sockaddr*)&server_address,sizeof(server_address))< 0)
			{
				throw std::out_of_range("bind() failed\n");
			}
		}
		//必须先置end_flag为true
		//因为ServiceUdpGo函数先获得锁
		void End()
		{
#if defined(Csz_test)
			std::cout<<"AdcanceUdp::End()\n";
#endif
			end_flag= true;
		}
		//线程调用函数
		//参数为线程池，用来存放请求任务
		//recvfrom 接收请求消息,消息大小需要比实际大小少1，因为要用来存放最后一个null
		//因为udp是有界，所以在接收消息时，当接收消息大小< 缓冲大小时，多余的数据将被
		//悄悄的丢弃，利用这个我们调用Check()检查mess.data[1]是否为空，若不时则是非法用户
		void ServiceUdpGo(std::shared_ptr<Csz_ThreadPool::ThreadPool> T_pool)
		{
			//多路复用
			fd_set sock_set; //创建描述符
			int max_desc= sock+ 1; //设置最大描述符
			int code; //记录select返回值 -1为错误，0为超时
#if defined(Csz_test)
			std::cout<<"AdvanceUdp::ServiceUdpGo() start\n";
#endif
			while (!end_flag)
			{
				FD_ZERO(&sock_set); //清零
				FD_SET(sock,&sock_set); //设置描述符
				//时间结构必须每次都重置，否则下次清零
				struct timeval time_out;
				time_out.tv_sec= TIMEOUT; //秒
				time_out.tv_usec= 0; //微秒
				memset(&mess,0,sizeof(mess));
				socklen_t addr_len= sizeof(mess.client_addr);
				if ((code= select(max_desc,&sock_set,NULL,NULL,&time_out))<= 0)
				{
					if (-1== code)
					{
						std::cerr<<"AdvanceUdp::ServiceUdpGo() select return -1\n";
						exit(0);
					}
#if defined(Csz_test)
					//std::cout<<"AdvanceUdp::ServiceUdpGo() time out\n";
#endif
					//continue;
					//超时后若直接进行下一次操作，recvfrom则不会运行，
					//下次select也是等待超时，而没收到上次信号量
				}
				if (FD_ISSET(sock,&sock_set))
				{
					if (recvfrom(sock,mess.data,sizeof(mess.data)-1,0,(struct sockaddr*)&mess.client_addr,&addr_len)< 0)
					{
#if !defined(Csz_O1)
						std::cerr<<"recvfrom< 0\n";
#endif
						continue;
					}
				}
				else
				{
					continue;
				}
				if (Check())
				{
					mess.sentry= RetNum(mess.data[0]);
					mess.sock= GetBroadCastSock();
					T_pool->AddTask(UdpBroadCast(),std::move(mess));
				}
				else
				{
					//AOP
					//记录错误请求ip
					//std::cerr<<"error request from ip"
#if !defined(Csz_O1)
					char client_addr[16]={0};
					inet_ntop(AF_INET,&mess.client_addr.sin_addr.s_addr,client_addr,sizeof(client_addr));
					std::cerr<<"error request from ip "<<client_addr<<" port "<<ntohs(mess.client_addr.sin_port)<<"\n";
#endif
				}
			}
#if defined(Csz_test)
			std::cout<<"AdvanceUdp::ServiceUdpGo() stop\n";
#endif
		}
};

#endif //Csz_ServiceUdp_H
