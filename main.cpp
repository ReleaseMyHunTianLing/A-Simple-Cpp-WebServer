#include <assert.h>
#include <signal.h>

#include "pthreadPool.h"
#include "http_conn.h"

#define MAX_EVENT  10000
#define PORT 8000

extern int addfd(int epollfd,int fd,bool oneshot);
extern void sys_err(char* str,int num);

void addsig(int sig,void(handler)(int))
{
	struct sigaction sa;
	memset(&sa,'\0',sizeof(sig));
	sigfillset(&sa.sa_mask);
	sa.sa_flags |= SA_RESTART;
	sa.sa_handler = handler;
	assert(sigaction(sig,&sa,NULL) != -1);
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
	Http_conn* users = new Http_conn[MAX_FD];
	assert(users);
	static int user_count = 0;

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

	epoll_event events[MAX_EVENT];
	int epollfd = epoll_create(5);
	if(epollfd == -1)
		sys_err((char*)"epoll_create",1);
	addfd(epollfd,listenfd,false);
	Http_conn::m_epollfd = epollfd;

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
				users[connfd].init(connfd,clieaddr);
				//printf("New client_fd %d\n",connfd);
			}
			else if(events[i].events & (EPOLLRDHUP |EPOLLHUP |EPOLLERR)){
				//有错误，关闭
				//printf("EPOLLHUP\n");
				users[sockfd].close_conn();
			}
			else if(events[i].events & EPOLLIN){                   //有输入
				//printf("EPOLLIN:cliefd = %d\n",sockfd);
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
				//proactor
				if(!users[sockfd].write())
					users[sockfd].close_conn();
				*/
				pool -> append(&users[sockfd]);
			}
			else 
			{}
		}
	}
	close(epollfd);
	close(listenfd);
	delete [] users;
	delete pool;
	return 0;
}