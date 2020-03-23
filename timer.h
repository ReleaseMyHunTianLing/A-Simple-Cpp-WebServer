#ifndef TIMER_H
#define TIMER_H

#include<list>
#include<time.h>
#include<assert.h>
#include"http_conn.h"

class Timer
{
    public:
	Timer() : prev(NULL),next(NULL) { }

	time_t expire;    //超时时间，使用绝对时间
	void (*cb_func)(Http_conn* request);  //回调函數
    Http_conn* m_http_conn;
	Timer* prev;
	Timer* next;
};
/*
//比较函数，用于小顶堆，小的在前
struct cmp{
    bool operator() (Timer* a,Timer* b){
        assert(a != NULL && b!= NULL);
        return a->expire > b->expire;
    }
};
*/
class Timer_list
{
public:
   //std::priority_queue<Timer*,std::vector<Timer*>,cmp>timer_queue; 
	Timer_list() :head(NULL) ,tail(NULL) { }
	~Timer_list()
	{
		Timer* tmp = head;
		while(tmp) {
			head = tmp->next;
       			delete tmp;
				tmp = head;
		}
	}
   void add_timer(Timer* timer);
   void del_timer(Timer* timer);
   void adjust_timer(Timer* timer);
   void tick();
private:
   void add_timerA(Timer* timer,Timer* lst_head);
private:
	Timer* head;
	Timer* tail;
};

#endif