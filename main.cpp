#include <assert.h>
#include <signal.h>

#include "pthreadPool.h"
#include "http_conn.h"
#include "timer.h"

#define MAX_EVENT  10000
#define PORT 8000
int TIMEOUT=5;

extern void addfd(int epollfd,int fd,bool oneshot);
extern int  sys_err(char* str,int num);
extern void setnonblocking(int fd);


Timer_list timer_list; 
static int pipefd[2];            //1写0读
static int user_count = 0;

void time_handler()
{
	timer_list.tick();
	alarm(TIMEOUT);            //重新定时 
}

void sig_handler(int sig)     //定时信号处理函数
{
    int save_err = errno;
    int msg = sig;                                                                                                                
    send(pipefd[1],(char*)&msg,1,0);  //统一事件源
    errno = save_err;
}


void addsig(int sig,void(handler)(int))
{
	struct sigaction sa;
	memset(&sa,'\0',sizeof(sig));
	sigfillset(&sa.sa_mask);
	sa.sa_flags |= SA_RESTART;
	sa.sa_handler = handler;
	assert(sigaction(sig,&sa,NULL) != -1);
}

void cb(Http_conn* re)
{
	re->close_conn();
}

int main()
{
	ThreadPool<Http_conn>* pool = NULL;	
	try
	{
		pool = new ThreadPool<Http_conn>;
	}
	catch(...)
	{
		printf("pool fail\n");
		return 0;
	}
	//忽略sigpipe
	addsig(SIGPIPE,SIG_IGN);
	//定时器函数
	addsig(SIGALRM,sig_handler);

	Http_conn* users = new Http_conn[MAX_FD];
	assert(users);

	int listenfd = socket(AF_INET,SOCK_STREAM,0);
	if(listenfd == -1)
		sys_err((char*)"socket",1);

	int ret = 0;
	struct sockaddr_in address;
	bzero(&address,sizeof(address));
	address.sin_family = AF_INET;
	address.sin_port = htons(PORT);
	address.sin_addr.s_addr = htonl(INADDR_ANY);

	ret = bind(listenfd,(struct sockaddr*)&address,sizeof(address));
	if(ret != 0)
		sys_err((char*)"bind",1);

	ret = listen(listenfd,5);
	if(ret != 0)
		sys_err((char*)"listen",1);

	ret = socketpair(AF_UNIX,SOCK_STREAM,0,pipefd);
	if(ret != 0)
		sys_err((char*)"socketpair",1);
	setnonblocking(pipefd[1]);

	epoll_event events[MAX_EVENT];
	int epollfd = epoll_create(5);
	if(epollfd == -1)
		sys_err((char*)"epoll_create",1);
	addfd(epollfd,listenfd,false);
	addfd(epollfd,pipefd[0],false);
	Http_conn::m_epollfd = epollfd;

	bool timeout = false;                 //定时
	alarm(TIMEOUT);

	while(true){
		int number = epoll_wait(epollfd,events,MAX_EVENT,-1);
		if((number < 0) && (errno != EINTR))
			sys_err((char*)"epoll_wait",1);

		for(int i = 0;i < number;i++){
			int sockfd = events[i].data.fd;
			/*有新客户机*/
			if(sockfd == listenfd){                 
				struct sockaddr_in clieaddr;
				socklen_t addrlen = sizeof(clieaddr);
				int connfd = accept(listenfd,(struct sockaddr*)&clieaddr,&addrlen);
				if(connfd < 0){
					continue;
				}
				if(Http_conn::m_user_count >= MAX_FD){
					printf("Server Busy\n");
					send(connfd,(char*)"ServerBusy",strlen("ServerBusy"),0);
					close(connfd);
					continue;
				}
				//正常连接
				users[connfd].init(connfd,clieaddr);
				time_t timeout = time(NULL)+3*TIMEOUT;
				Timer* timer = new Timer;
				timer->expire = timeout;
				timer->m_http_conn = &users[connfd];
				timer->cb_func = cb; 
				users[connfd].m_timer = timer;
				timer_list.add_timer(timer);
			}
			else if(events[i].events & (EPOLLRDHUP |EPOLLHUP |EPOLLERR)){
				//有错误，关闭
				users[sockfd].close_conn();
			}
			else if(sockfd == pipefd[0]){
				int sig;
				char signals[5];
				ret = recv(pipefd[0],signals,sizeof(signals),0);                                                                  
                if(ret <= 0)
                    continue;
				else{
					for(int j = 0;j < ret;j++)  {
                        switch(signals[i])
                        {
                            case SIGALRM:
                                {
                                    timeout = true;  
                                    break;
                                }
								//
                         }
                    }
				}

			}
			else if(events[i].events & EPOLLIN){                   //有输入
				/*
				//proactor
				if( users[sockfd].read() ){                        //读取成功，添加任务队列
					pool -> append(&users[sockfd]);
				}
				else{ 
					//printf("Client %d break\n",sockfd);
					users[sockfd].close_conn();
				}
				*/
				pool -> append(&users[sockfd]);
			}
			else if(events[i].events & EPOLLOUT){
				/*
				if(!users[sockfd].write())
					users[sockfd].close_conn();
				*/
				pool -> append(&users[sockfd]);
			}
			if(timeout){
				time_handler();
				timeout = false;
			}
		}
	}
	close(epollfd);
	close(listenfd);
	delete [] users;
	delete pool;
	return 0;
}