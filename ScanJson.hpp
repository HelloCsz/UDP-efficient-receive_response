#include <fstream>
#include <vector>
#include <memory>
#include <stdexcept> //std::invalid_argument
#include <string>
#include <limits>
#ifndef Csz_ScanJson_H
#define Csz_ScanJson_H

//#define Csz_test

struct ScanJson
{
	std::shared_ptr<std::vector<uint32_t>> operator()(const char *T_file_name)
	{
		std::ifstream file(T_file_name);
		if (!file.is_open())
		{
			throw std::invalid_argument(std::string(T_file_name)+"open failed");
		}
		std::shared_ptr<std::vector<uint32_t>> temp(std::make_shared<std::vector<uint32_t>>());
		temp->reserve(16);
#if defined(Csz_test)
		std::cerr<<"capacity: "<<temp->capacity()<<",size= "<<temp->size()<<"\n";
#endif
		while (file)
		{
			temp->push_back(file.tellg());
			file.ignore(std::numeric_limits<std::streamsize>::max(),'\n');
			if (-1== file.peek())
				break;
		}
#if defined(Csz_test)
		for (auto &value : *temp)
			std::cout<<value<<"\n";
#endif
		file.close();
		//temp->shrink_to_fit(); //造成迭代器失效，没必要重新申请一块空间
		return temp;
	}
};
#endif
