#ifndef HTTP_CONN_H
#define HTTP_CONN_H
#include <sys/types.h>
#include <sys/socket.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/epoll.h>
#include <string.h>
#include <errno.h>
#include <algorithm>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <sys/uio.h>
#include <time.h>
//#include "timer.h"
class Timer;

#define MAX_FD 65536
class Http_conn
{
	public:
		int m_flag;    //1是读就绪，0是写就绪

		static const int READ_BUFFER_SIZE = 4096;
		static const int WRITE_BUFFER_SIZE = 4096;
		static const int FILENAME_LEN = 200;

		//http请求方法,仅支持get
		enum METHOD { GET=0,POST,HEAD,PUT,DELETE};
		//解析客户请求时的状态
		enum CHECK_STATE { CHECK_STATE_REQUESTLINE=0,
						   CHECK_STATE_HEADER,
						   CHECK_STATE_CONTENT};
		//行的读取状态，分别为 已读一行，错误行，未读完一行
		enum LINE_STATE { LINE_OK=0,LINE_BAD,LINE_OPEN};
		//服务器处理http请求的结果
		enum HTTP_CODE {NO_REQUEST,GET_REQUEST,BAD_REQUEST,
						NO_RESOURCE,FORBIDDEN_REQUEST,FILE_REQUEST,
						INTERNAL_ERROR,CLOSED_CONNECTION,POST_FILE};

		HTTP_CODE read_ret;
		//所处状态
		CHECK_STATE m_check_state;
		//请求方法		  
		METHOD m_method;                  
	
		/*http请求解析函数*/
		LINE_STATE parse_line();   
		HTTP_CODE parse_request_line(char* text);
		HTTP_CODE parse_headers(char* text);
		HTTP_CODE parse_content(char* text);
		//主状态机入口函数
		HTTP_CODE process_read();
		//处理get请求
		HTTP_CODE do_request();
		void post_response();
		char post_buf[1024];
		void unmap();
		char* get_line(){return m_read_buf + m_start_line;}
		//读取客户数据
		bool read();				
		//写http响应
		bool write();               

		bool process_write(HTTP_CODE ret);
	private:
		// 读缓冲区
		char m_read_buf[READ_BUFFER_SIZE];
		//读缓冲区中已读的最后一个字节的下一个位置（下标）
		int m_read_idx;
		//当前正分析的字节在读缓冲区中的位置
		int m_checked_idx;
		//当前正解析行的起始位置
		int m_start_line;
		//消息体长度
		int m_content_length;
		//是否长连接
		bool m_linger;
		//写缓冲区
		char m_write_buf[WRITE_BUFFER_SIZE];
		//写缓冲区中待发送的字节数
		int m_write_idx;
		//目标文件的状态
		struct stat m_file_stat; 
		//目标文件被mmap映射的起始位置
		char* m_file_address;
		//writev操作所需参数
		struct iovec m_iovec[2];
		int m_iovec_count;                

		//资源定位符,这里是客户请求的目标文件名
		char* m_url;  
		//http版本号                    
		char* m_version;      
		//主机名            
		char* m_host;					  
		//客户请求文件的完整目录，其值等于doc_root + m_url
		char m_real_file[FILENAME_LEN];


		bool add_response(const char* format,...);
		bool add_state_line(int state,const char* title);
		bool add_headers(int content_len);
		bool add_content_length(int content_len);
		bool add_linger();
		bool add_blank_line();
		bool add_content(const char* content);

	public:
		void process();
		void close_conn();
		void init(int sockfd,struct sockaddr_in address);
		void init();

		static int m_epollfd;					//所有事件注册的同一个内核epoll事件表
		static int m_user_count;         //接入的客户端数目，即用户数量

		int m_sockfd;								//该http连接的socket和对方socket的地址  
		struct sockaddr_in m_address;

		Timer* m_timer;                   //定时器相关
		//Timer* get_timer(){return m_timer;}
		//void set_timer(Timer* timer){m_timer = timer;}
};

#endif