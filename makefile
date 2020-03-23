
httpServ: main.o http_conn.o timer.o
	g++ main.o http_conn.o timer.o -o httpServ -lpthread

main.o:main.cpp pthreadPool.h  
	g++ -c main.cpp

http_conn.o: http_conn.cpp  http_conn.h 
	g++ -c http_conn.cpp 
	
timer.o: timer.cpp timer.h
	g++ -c timer.cpp

clean:
	rm *.o
