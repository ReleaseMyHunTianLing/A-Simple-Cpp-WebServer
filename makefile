
httpServ: main.o http_conn.o
	g++ main.o http_conn.o -o httpServ -lpthread

main.o:main.cpp pthreadPool.h  
	g++ -c main.cpp

http_conn.o: http_conn.cpp  http_conn.h
	g++ -c http_conn.cpp

clean:
	rm *.o
