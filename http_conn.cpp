#include"http_conn.h"
#include <stdarg.h>
#include <sys/wait.h>


//http响应信息
const char* ok_200_title = "OK";
const char* error_400_title = "Bad Request";
const char* error_400_form = "Your request has bad syntax.\n";
const char* error_403_title = "Forbidden";
const char* error_403_form = "You do not have permission,\n";
const char* error_404_title = "Not Found";
const char* error_404_form = "The requested file was not found.\n";
const char* error_500_title = "Internal Error";
const char* error_500_form = "Mayebe you coule try later.\n";
//根目录
const char* doc_root = "./www";

Http_conn* users = new Http_conn[MAX_FD];

int Http_conn::m_epollfd = -1;						//初始化类的静态成员变量（必须在类外进行）
int Http_conn::m_user_count = 0;

void setnonblocking(int fd)
{
	int oldopt = fcntl(fd,F_GETFL);
	int newopt = oldopt | O_NONBLOCK;
	fcntl(fd,F_SETFL,newopt);
}                                                                           

void addfd(int epollfd,int fd,bool oneshot)
{
	epoll_event event;
	event.data.fd = fd;
	event.events = EPOLLIN | EPOLLET |EPOLLRDHUP;
	if(oneshot)
		event.events |= EPOLLONESHOT;
	epoll_ctl(epollfd,EPOLL_CTL_ADD,fd,&event);
	setnonblocking(fd);
}
void removefd(int epollfd,int fd)
{
	epoll_ctl(epollfd,EPOLL_CTL_DEL,fd,0);
	close(fd);
}
void modfd(int epollfd,int fd,int ev)
{
	epoll_event event;
	event.data.fd = fd;
	event.events = ev | EPOLLET |EPOLLONESHOT | EPOLLRDHUP;
	epoll_ctl(epollfd,EPOLL_CTL_MOD,fd,&event);
}

void sys_err(char* str,int num)
{
	perror(str);
	exit(1);
}


void Http_conn::close_conn()
{
	removefd(m_epollfd,m_sockfd);
	m_sockfd = -1;
	Http_conn::m_user_count--;
}
void Http_conn::init(int connfd,struct sockaddr_in address)
{
	m_sockfd = connfd;
	//printf("init m_sockfd:%d\n",m_sockfd);
	m_address = address;
	addfd(m_epollfd,connfd,true);
	Http_conn::m_user_count++;

	init();
}
void Http_conn::init()
{
    /*读状态*/
    m_flag = 1;
    read_ret = NO_REQUEST;

	m_method = GET;
	m_check_state = CHECK_STATE_REQUESTLINE;
	m_read_idx = 0;
	m_checked_idx = 0;
	m_start_line = 0;
	m_write_idx = 0;
	m_content_length = 0;
	m_linger = false;
	m_host = 0;
	memset(m_read_buf,'\0',READ_BUFFER_SIZE);
	memset(m_write_buf,'\0',WRITE_BUFFER_SIZE);
	memset(m_real_file,'\0',FILENAME_LEN);
	
}


//读取客户端数据
bool Http_conn::read()
{
    if(m_read_idx >= READ_BUFFER_SIZE)
        return false;
    
    int read_bytes = 0;
    while(true){
        read_bytes = recv(m_sockfd,m_read_buf + m_read_idx,
                          READ_BUFFER_SIZE - m_read_idx,0);
        if(read_bytes == -1){
            if(errno == EAGAIN || errno == EWOULDBLOCK){
                break;
            }
            return false;
        }
        else if(read_bytes == 0){
            return false;
        }
        m_read_idx += read_bytes;
    }
    return true;
}
//解析出一行
Http_conn::LINE_STATE Http_conn::parse_line()
{
    char temp;
    for(;m_checked_idx < m_read_idx;m_checked_idx++){
        temp = m_read_buf[m_checked_idx];
        if(temp == '\r'){
            if((m_checked_idx + 1) == m_read_idx)
                return LINE_OPEN;
            else if (m_read_buf[m_checked_idx+1] == '\n'){
                m_read_buf[m_checked_idx++] = '\0';
                m_read_buf[m_checked_idx++] = '\0';
                return LINE_OK;
            }
            return LINE_BAD;
        }    
        else if(temp == '\n'){
            if(m_checked_idx > 1 && m_read_buf[m_checked_idx-1] == '\r'){
                m_read_buf[m_checked_idx-1] = '\0';
                m_read_buf[m_checked_idx++] = '\0';
                return LINE_OK;   
            }
            return LINE_BAD;
        }
    }
    return LINE_OPEN;
}
//解析http请求行，获取请求方法，url,http版本号
Http_conn::HTTP_CODE Http_conn::parse_request_line(char* text)
{
    m_url = strpbrk(text," \t");
    if(!m_url)
        return BAD_REQUEST;
    
    *m_url++ = '\0'; 
    //char * p = '\0'和char *p=NULL等价，是指针的一种安全状态；
    //这时p值虽然是0，但进行写和读操作都是非法的，系统不会认可，从而保证了安全。

    char* method = text;
    if(strcasecmp(method,"GET") == 0)
        m_method = GET;
    else if(strcasecmp(method,"POST") == 0)
        m_method = POST;
    else 
        return BAD_REQUEST;

    m_url += strspn(m_url," \t");
    m_version = strpbrk(m_url," \t");
    if(!m_version)
        return BAD_REQUEST;
    *m_version++ = '\0';
    m_version += strspn(m_version," \t");
    if(strcasecmp(m_version,"HTTP/1.1") != 0)
        return BAD_REQUEST;
    if(strncasecmp(m_url,"http://",7) == 0){
        m_url += 7;
        m_url = strchr(m_url,'/');
    }
    if(!m_url || m_url[0] != '/'){
        return BAD_REQUEST;
    }
    if(strcasecmp(m_url,"/") == 0){
        //printf("------------------------------------------m_url == /");
        m_url = (char *)"/index.html";
    }
    //printf("!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!The request url is %s\n",m_url);
    m_check_state = CHECK_STATE_HEADER;
    return NO_REQUEST;
}
//解析头部字段
Http_conn::HTTP_CODE Http_conn::parse_headers(char* text)
{
    //遇到空行，说明头部字段解析完毕
    if(text[0] == '\0'){
        if(m_content_length != 0){ //该http请求含有消息体
            m_check_state = CHECK_STATE_CONTENT;
            return NO_REQUEST;
        }

        return GET_REQUEST;
    }     
    //处理Connection头部字段
    else if(strncasecmp(text,"Connection:",11) == 0){
        text += 11;    
        text += strspn(text," \t");
        if(strcasecmp(text,"keep-alive") == 0)
            m_linger = true;
    }
    //处理Content_Length头部字段
    else if(strncasecmp(text,"Content-Length:",15) == 0){
        text += 15;
        text += strspn(text," \t");
        m_content_length = atol(text);
    }
    //处理Host
    else if(strncasecmp(text,"Host:",5) == 0){
        text += 5;
        text += strspn(text," \t");
        m_host = text; 
    }
    //带续。。。
    //else 
       // printf("SORRY!!!!unknow header %s\n",text);
    
    return NO_REQUEST;    

}
//解析消息体
Http_conn::HTTP_CODE Http_conn::parse_content(char* text)                
{
    if(m_read_idx >= (m_content_length + m_checked_idx) ){
        text[m_content_length] = '\0';
        //printf("PARSE_CONTENT : %s\n",text);
        
        strcpy(post_buf,text);

        post_buf[strlen(post_buf)+1] = '\0';
        //printf("!!!!!!!!POST_BUF : %s\n",post_buf);
        return POST_FILE;
    }
    return NO_REQUEST;
}

//主状态机，入口函数
Http_conn::HTTP_CODE Http_conn::process_read()
{
    LINE_STATE line_state = LINE_OK;
    HTTP_CODE ret = NO_REQUEST;
    char* text = NULL;

    while( (m_check_state == CHECK_STATE_CONTENT && line_state == LINE_OK)
            || (line_state = parse_line()) == LINE_OK)
    {
        text = get_line();
        m_start_line = m_checked_idx;
        //printf("get one http line : %s\n",text);
        //if(m_check_state == CHECK_STATE_CONTENT)
            //printf("m_check state == CHECK_STATE_CONTENT\n");

        switch (m_check_state)
        {
            case CHECK_STATE_REQUESTLINE:
            {
                ret = parse_request_line(text);
                if(ret == BAD_REQUEST)
                    return BAD_REQUEST;
                break;
            }
            case CHECK_STATE_HEADER:
            {
                ret = parse_headers(text);
                if(ret == BAD_REQUEST)
                    return BAD_REQUEST;
                else if(ret == GET_REQUEST){
                    if(m_method == GET){
                        return do_request();
                    }
                }
                break;
            }
            case CHECK_STATE_CONTENT:
            {
                ret = parse_content(text);
                if(ret == POST_FILE)
                    return POST_FILE;
                break;
            }
            default:
                return INTERNAL_ERROR;
        }
    }
    return NO_REQUEST;
}
void Http_conn::post_response()
{
    strcpy(m_real_file,doc_root);
    strcpy(m_real_file,m_url);
    if(fork() == 0){
        char buf[1024];
        getcwd(buf,1024);
        //printf("pwd : %s\n",buf);
        strcat(buf,m_real_file);
        //printf("post_buf : %s\n",post_buf);
        //printf("m_real_file : %s\n",m_real_file);
        //printf("m_sockfd : %d\n",m_sockfd);
        if(dup2(m_sockfd,STDOUT_FILENO) == -1)
            sys_err((char*)"dup2",1);
        if(execl(buf,post_buf,NULL) == -1)
            sys_err((char*)"execl",1);
    }
    wait(NULL);
}

//处理客户请求，将客户请求的文件映射到m_file_address
Http_conn::HTTP_CODE Http_conn::do_request()
{
    strcpy(m_real_file,doc_root);
    int len = strlen(doc_root);
    strncpy(m_real_file + len,m_url,FILENAME_LEN - len - 1);
    if(stat(m_real_file, &m_file_stat) < 0)    
        return NO_RESOURCE;
    if(!(m_file_stat.st_mode & S_IROTH))
        return FORBIDDEN_REQUEST;
    if(S_ISDIR(m_file_stat.st_mode))
        return BAD_REQUEST;

    int fd = open(m_real_file, O_RDONLY);
    m_file_address = (char*)mmap(NULL,m_file_stat.st_size,
                                PROT_READ,MAP_PRIVATE,fd,0);
    close(fd);
    return FILE_REQUEST;
}

//munmap
void Http_conn::unmap()
{
    if(m_file_address){
        munmap(m_file_address,m_file_stat.st_size);
        m_file_address = NULL;
    }
}

//发送http响应
bool Http_conn:: write()
{
    int temp = 0;
    int bytes_have_send = 0;
    int bytes_to_send = m_write_idx;
    if(bytes_to_send == 0){              //没有要发送的字节了
        modfd(m_epollfd,m_sockfd,EPOLLIN);
        init();
        return true;    //发送成功
    }
    while(1){
        temp = writev(m_sockfd,m_iovec,m_iovec_count);
        if(temp <= -1){
            if(errno == EAGAIN){        //非阻塞模式下进行了阻塞操作：如tcp写缓冲区没有空间
                modfd(m_epollfd,m_sockfd,EPOLLOUT);
                return true;
            }
            unmap();
            return false;
        }
        bytes_to_send -= temp;
        bytes_have_send += temp;
        if(bytes_to_send <= bytes_have_send){
            //http响应发送成功，根据是否长连接决定是否立即关闭连接
            unmap();
            if(m_linger){           //长链接
                init();     
                modfd(m_epollfd,m_sockfd,EPOLLIN);
                return true;
            }
            else{                 //不是长链接
                modfd(m_epollfd,m_sockfd,EPOLLIN);
                return false;
            }
        }

    }
}


//向写缓冲区写入待发数据
bool Http_conn::add_response(const char* format,...)
{
    if(m_write_idx >= WRITE_BUFFER_SIZE)
        return false;
    va_list arg;
    va_start(arg,format);
    int len = vsnprintf(m_write_buf+m_write_idx,WRITE_BUFFER_SIZE-1-m_write_idx,
                        format,arg);
    if(len >= (WRITE_BUFFER_SIZE-1-m_write_idx))
        return false;
    m_write_idx += len;
    va_end(arg);
    return true;
}

//添加响应状态行
bool Http_conn::add_state_line(int state,const char* title)
{
    return add_response("%s %d %s\r\n","HTTP/1.1",stat,title);
}

//添加响应头部字段
bool Http_conn::add_headers(int content_len)
{
    add_content_length(content_len);
    add_linger();
    add_blank_line();
}

//添加Content-Length: 字段
bool Http_conn::add_content_length(int content_len)
{
    return add_response("Content-Length: %d\r\n",content_len);
}

//添加Connection字段
bool Http_conn::add_linger()
{
    return add_response("Connection: %s\r\n",(m_linger == true)?"keep-alive":"close");
}


//添加空行
bool Http_conn::add_blank_line()
{
    return add_response("%s","\r\n");
}

//添加文本
bool Http_conn::add_content(const char* content)
{
    return add_response("%s",content);
}

//根据服务器处理http请求的结果，决定写给客户端响应的
bool Http_conn::process_write(HTTP_CODE ret)
{
    switch(ret)
    {
        case INTERNAL_ERROR:
        {
            add_state_line(500,error_500_title);
            add_headers(strlen(error_500_form));
            if(!add_content(error_500_form))
                return false;
            break;
        }
        case BAD_REQUEST:
        {
            add_state_line(400,error_400_title);
            add_headers(strlen(error_400_form));
            if(!add_content(error_400_form))
                return false;
            break;
        }
        case NO_RESOURCE:
        {
            add_state_line(404,error_404_title);
            add_headers(strlen(error_404_form));
            if(!add_content(error_404_form))
                return false;
            break;
        }
        case FORBIDDEN_REQUEST:
        {
            add_state_line(403,error_403_title);
            add_headers(strlen(error_403_form));
            if(!add_content(error_403_form))
                return false;
            break;
        }
        case FILE_REQUEST:
        {
            add_state_line(200,ok_200_title);
            if(m_file_stat.st_size != 0){
                add_headers(m_file_stat.st_size);
                m_iovec[0].iov_base = m_write_buf;
                m_iovec[0].iov_len = m_write_idx;
                m_iovec[1].iov_base = m_file_address;
                m_iovec[1].iov_len = m_file_stat.st_size;
                m_iovec_count = 2;
                return true;
            }
            else{
                const char* ok_str = "<html><boday></body></html>";
                add_headers(strlen(ok_str));
                if(!add_content(ok_str))
                    return false;
            }
        }
        case POST_FILE:
        {
            post_response();
            return true;
        }
        defalt:
        {
            return false;
        }

        m_iovec[0].iov_base = m_write_buf;
        m_iovec[0].iov_len = m_write_idx;
        m_iovec_count = 1;
        return true;
    }
}

//线程池入口函数
void Http_conn::process()
{
    //HTTP_CODE read_ret;
    if(m_flag == 1){
        if(!read()){
            close_conn();
        }
        read_ret = process_read();
        if(read_ret == NO_REQUEST){
            modfd(m_epollfd,m_sockfd,EPOLLIN);
            return;
        }
        /*变为写状态*/
        m_flag = 0;             
        modfd(m_epollfd,m_sockfd,EPOLLOUT);
    }
    else {
        bool write_ret = process_write(read_ret);
        if(!write_ret){
            close_conn();
        }

        if(!write()){
            close_conn();
        }
        //modfd(m_epollfd,m_sockfd,EPOLLOUT);
    }
}

/*
//proactor
void Http_conn::process()
{
	HTTP_CODE read_ret = process_read();
	if(read_ret == NO_REQUEST){
		modfd(m_epollfd,m_sockfd,EPOLLIN);
		return;
	}

	bool write_ret = process_write(read_ret);
	if(!write_ret){
		close_conn();
	}
	modfd(m_epollfd,m_sockfd,EPOLLOUT);
	
}
*/