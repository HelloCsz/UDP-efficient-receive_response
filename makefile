name=UDP	
CC=g++	
#g++ 6.3.0
CFLAGS= -std=c++11		
obj=c.o	
$(name):$(obj)	
	$(CC) -pthread -o $(name) $(obj)	
c.o: c.cpp Any.hpp ScanJson.hpp ServiceUdp.h TaskList.hpp ThreadPool.hpp
	$(CC) $(CFLAGS) -c c.cpp	
clean:
	-rm *.o	
