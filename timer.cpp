#include"timer.h"
void Timer_list::add_timer(Timer* timer)
{
	if(!timer){
		return;
    }
	mutex.lock();
	if(!head){
	    head = timer;
		tail = timer;
		mutex.unlock();
		return;
	}
	if(timer -> expire < head -> expire){   //如果定時器時間小於頭結點時間，則插入頭結點前，否則調用重載函數
		timer -> next = head;
		head -> prev = timer;
		head = timer;
		mutex.unlock();
		return;
	}
	add_timerA(timer,head);
	mutex.unlock();
}

void Timer_list::add_timerA(Timer* timer,Timer* lst_head)
{
	Timer* prev = lst_head;
	Timer* tmp = prev -> next;
	while(tmp){
		if(timer -> expire < tmp -> expire){      //遍歷頭結點之後的鏈表，直到找到一個超時時間大於目標定時器時間的結點
			prev -> next = timer;
			timer -> next = tmp;
			tmp -> prev = timer;
			timer -> prev = prev;
			break;
		}
		prev = tmp;
		tmp = tmp -> next;
	}
			
	if(!tmp){                                   //直到尾結點都沒有找到超時時間大於目標定期器時間的，把它插入鏈表尾巴
		prev -> next = timer;
		timer -> prev = prev;
		timer -> next =NULL;
		tail = timer;
	}
}

void Timer_list::del_timer(Timer* timer)
{
	
	if(!timer)
		return;
	mutex.lock();
	if(timer == head && timer == tail){
		delete timer;
		head = NULL;
		tail == NULL;
		mutex.unlock();
		return;
	}
	if(timer == head){
		head = head->next;
		head->prev = NULL;
		delete timer;
		mutex.unlock();
		return;
	}
	if(timer == tail){
		tail = timer -> prev;
		timer -> prev ->next = NULL;
		delete timer;
		mutex.unlock();
		return;
	}
	timer -> prev ->next = timer->next;
	timer -> next ->prev = timer -> prev;
	delete timer;
	mutex.unlock();	
}

void Timer_list::adjust_timer(Timer* timer)
{
	
    if(!timer)
		return;
	Timer* tmp = timer->next;
	if(!tmp || tmp->expire > timer->expire){
		return;
	}
	mutex.lock();
	if(head == timer){
		head = head->next;
		head->prev = NULL;
		timer->prev = NULL;
		add_timerA(timer,head);
	}
	else{
		timer -> prev ->next = timer->next;
		timer -> next ->prev = timer->prev;
		add_timerA(timer,timer->next);
	}
	mutex.unlock();
}

void Timer_list::tick()
{
	if(!head) {
		return;
	}
	time_t cur = time(NULL);  //当前时间
	Timer* tmp = head;
	while(tmp){         //从头结点开始处理直到遇到一个沒有到期的定时器
		if(cur < tmp -> expire)	
			break;

		tmp -> cb_func(tmp -> m_http_conn);  //回调函数
		head = head->next;
		if(head) {
			head -> prev = NULL;
		}
		delete tmp;
		tmp = head;
	}
}
