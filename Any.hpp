#include <memory> //unique_prt
#include <typeindex> //type_index 

#ifndef Csz_Any_H
#define Csz_Any_H

//擦除掉类型，利用在构造函数时，传进所要擦除的类型以及值，用指针
//进行记录值，用type_index记录类型，而在需要返回类型的值时，用模板
//继续传如类型，与type_index比较若相等则进行强制类型转换,dynamic_cast

struct Any 
{
	private:
		struct Base; //接口
		using BasePtr= std::unique_ptr<Base>; //智能指针保证资源释放
		std::type_index index;  //记录当前保存的类型
		BasePtr ptr;  //记录当前类型的智能指针，get()返回指向的类型
	private:
		struct Base
		{
			//用虚函数保证继承关系的派生类能够调用析构函数
			//在实现多态时，当用基类操作派生类，在析构时防止只析构基类而不析构派生类
			virtual ~Base(){}
			virtual BasePtr Clone()const =0;
		};
		//保存值的类
		//模板特例化传进的是保持值的类型，而构造函数模板则是保证值传递进来是原始类型值
		//这样做的好处是可能是右值，也能剩下构造值的时间
		template <typename T>
		class Derived : public Base
		{
			public:
				T value;
			public:
				template <typename U>
				Derived(U &&T_value) : value(std::forward<U>(T_value)) 
				{
#if defined(Csz_test)
					std::cout<<"Any::Derived::constructor\n";
#endif
				}
#if defined(Csz_test)
				~Derived(){std::cout<<"Any::Derived::destructor\n";}
#endif
				BasePtr Clone()const
				{
#if defined(Csz_test)
					std::cout<<"Any::Derived::Clone()\n";
#endif
					return BasePtr(new Derived<T>(value));
				}
		};
		//辅助方法,调用Derived::Clone()变简单
		BasePtr Clone() const
		{
#if defined(Csz_test)
			std::cout<<"Any::Close()\n";
#endif
			if (nullptr!= ptr)
				return ptr->Clone();
			return nullptr;
		}
	public:
		//默认构造
		Any() : index(typeid(void)),ptr(nullptr){}
		//右值构造
		Any(Any &&T_other) : index(std::move(T_other.index)),ptr(std::move(T_other.ptr)){}
		//左值构造
		Any(const Any &T_other) : index(T_other.index),ptr(T_other.Clone()){}
		//值构造
		//用模板，限定模板类型为非Any类
		//std::decay 先移除T引用得到U，U为 remove_reference<T>::type
		//若是is_array<U>::value 为true,则修改类型type为 remove_extent<U>::type* 移除数组的一个维度，若是一维数组则是变量指针
		//若是is_function<U>::value为true,则修改类型type为 add_pointer<U>::type 也就是函数指针
		//剩下的修改type类型为 remove_cv<U>::type
		template <typename T,typename= typename std::enable_if<!std::is_same<typename std::decay<T>::type,Any>::value,T>::type>
		Any(T &&T_value) : index(typeid(typename std::decay<T>::type)),ptr(new Derived<typename std::decay<T>::type>(std::forward<T>(T_value))){}
#if defined(Csz_test)
		~Any(){std::cout<<"Any::destructor\n";}
#endif
		bool IsNull() const noexcept
		{
#if defined(Csz_test)
			std::cout<<"Any::IsNull()\n";
#endif
			return nullptr== ptr;
		}
		//查询当前保存的类型是否为函数特例化的类型
		template <typename T>
		bool Is() const noexcept
		{
#if defined(Csz_test)
			std::cout<<"Any::Is()\n";
#endif
			return std::type_index(typeid(T))== index;
		}
		template <typename T>
		T AnyCast()
		{
#if defined(Csz_test)
			std::cout<<"Any::AnyCast()\n";
#endif
			if (!Is<T>())
			{
#if !defined(Csz_O1)
				std::cerr<<"can not cast "<<index.name()<<" to "<<typeid(T).name()<<"\n";
#endif
				throw std::bad_cast();
			}
			//std::unique_ptr<T>.get() 返回智能指针指向T的指针,若没有则返回nullptr
			//std::unique<Base> ptr
			auto derived= dynamic_cast<Derived<T>*>(ptr.get());
			//若是值为类则必须在类里面写好移动构造函数，避免了资源释放两次
			//若值是标准类型，其实也是无所谓但养成好习惯，标准类型的move and copy效果是一样的
			return std::move(derived->value);
		}
		//copy赋值操作符
		Any& operator=(const Any &T_other)
		{
#if defined(Csz_test)
			std::cout<<"Any::operator= copy()\n";
#endif
			if (T_other.ptr== ptr)
				return *this;
			ptr= T_other.Clone();
			index= T_other.index;
			return *this;
		}
		//move赋值操作符
		Any& operator=(Any &&T_other)
		{
#if defined(Csz_test)
			std::cout<<"Any::operator= move\n";
#endif
			if (T_other.ptr== ptr)
				return *this;
			ptr=std::move(T_other.ptr);
			index= std::move(T_other.index);
			return *this;
		}
};
#endif //Csz_Any_H
