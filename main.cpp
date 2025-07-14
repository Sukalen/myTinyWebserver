#include<stdio.h>
#include<stdlib.h>
#include<sys/socket.h>
#include<arpa/inet.h>
#include<sys/epoll.h>
#include<unistd.h>
#include<errno.h>
#include<fcntl.h>
#include<string.h>

#include "./lock/locker.h"
#include "./threadpool/threadpool.h"
#include "./timer/lst_timer.h"
#include "./http/http_conn.h"
#include "./log/log.h"
#include "./CGImysql/sql_connection_pool.h"

#define MAX_FD 65536
#define MAX_EVENT_NUMBER 10000
#define TIMESLOT 5
#define TIMESLOT_TIMES 12

//#define ASYNLOG
#define SYNLOG


#define listenfdET

//#define listenfdLT

extern int addfd(int epollfd,int fd,bool is_et,bool one_shot);
extern int removefd(int epollfd,int fd);
extern int setnonblocking(int fd);

static int pipefd[2];
static sort_timer_lst timer_lst;
static int epollfd = 0;

void sig_handler(int sig)
{
	int save_errno = errno;
	int msg = sig;
	send(pipefd[1],(char*)&msg,sizeof(msg),0);
	errno = save_errno;
}

void addsig(int sig,void (*handler)(int),bool restart = true)
{
	struct sigaction sa;
	memset(&sa,'\0',sizeof(sa));
	sa.sa_handler = handler;
	if(restart)
	{
		sa.sa_flags|=SA_RESTART;
	}
	sigfillset(&sa.sa_mask);
	if(-1 == sigaction(sig,&sa,NULL))
	{
		LOG_ERROR("sigaction failed");
		Log::get_instance()->flush();
		exit(1);
	}
}

void timer_handler()
{
	timer_lst.tick();
	alarm(TIMESLOT);
}

void cb_func(client_data* user_data)
{
	if(NULL == user_data)
	{
		LOG_ERROR("user_data is NULL");
		Log::get_instance()->flush();
		exit(1);
	}
	epoll_ctl(epollfd,EPOLL_CTL_DEL,user_data->sockfd,0);
	close(user_data->sockfd);
	http_conn::m_user_count--;
	LOG_INFO("close fd %d",user_data->sockfd);
	Log::get_instance()->flush();
}

void show_error(int connfd, const char* info)
{
    printf("%s", info);
    send(connfd, info, strlen(info), 0);
    close(connfd);
}

int main(int argc, char** argv)
{
#ifdef ASYNLOG
    Log::get_instance()->init("ServerLog", 2000, 800000, 8);
#endif

#ifdef SYNLOG
    Log::get_instance()->init("ServerLog", 2000, 800000, 0);
#endif

    if (argc <= 1)
    {
        printf("usage: %s port_number\n", basename(argv[0]));
        exit(1);
    }

    int port = atoi(argv[1]);

    addsig(SIGPIPE, SIG_IGN);

    connection_pool* connpool = connection_pool::get_instance();
    //connpool->init("localhost", "root", "root", "websrvdb", 3306, 8);
	connpool->init("localhost", "webuser", "Web@123456!", "websrvdb", 3306, 8);

    threadpool<http_conn>* pool = NULL;
    try
    {
        pool = new threadpool<http_conn>(connpool);
    }
    catch (...)
    {
        return 1;
    }

    http_conn* users = new http_conn[MAX_FD];
    if(NULL == users)
	{
		LOG_ERROR("users is NULL");
		Log::get_instance()->flush();
		exit(1);
	}

    users->initmysql_result(connpool);

    int listenfd = socket(AF_INET, SOCK_STREAM, 0);
    if(listenfd < 0)
	{
		LOG_ERROR("socket failed,listenfd < 0");
		Log::get_instance()->flush();
		exit(1);
	}

    int ret = 0;
    struct sockaddr_in servaddr;
    bzero(&servaddr, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
    servaddr.sin_port = htons(port);

    int flag = 1;
    setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, &flag, sizeof(flag));
    ret = bind(listenfd, (struct sockaddr*)&servaddr, sizeof(servaddr));
    if( -1 == ret)
	{
		LOG_ERROR("bind failed");
		Log::get_instance()->flush();
		exit(1);
	}
    ret = listen(listenfd, 5); 
    if(-1 == ret)
	{
		LOG_ERROR("listen failed");
		Log::get_instance()->flush();
		exit(1);
	}

    struct epoll_event events[MAX_EVENT_NUMBER];
    epollfd = epoll_create(5);
    if(-1==epollfd)
	{
		LOG_ERROR("epoll create failed");
		Log::get_instance()->flush();
		exit(1);
	}

#ifdef listenfdET
    addfd(epollfd, listenfd, true, false);
#endif

#ifdef listenfdLT
	addfd(epollfd,listenfd,false,false);
#endif

    http_conn::m_epollfd = epollfd;

    ret = socketpair(AF_UNIX, SOCK_STREAM, 0, pipefd);
    if(-1==ret)
	{
		LOG_ERROR("socketpair create failed");
		Log::get_instance()->flush();
		exit(1);
	}
    setnonblocking(pipefd[1]);
    addfd(epollfd, pipefd[0], true, false);

    addsig(SIGALRM, sig_handler, false);
    addsig(SIGTERM, sig_handler, false);
    bool stop_server = false;

    client_data* users_timer = new client_data[MAX_FD];

    bool timeout = false;
    alarm(TIMESLOT);

    while (!stop_server)
    {
        int number = epoll_wait(epollfd, events, MAX_EVENT_NUMBER, -1);
        if (number < 0 && errno != EINTR)
        {
            LOG_ERROR("%s", "epoll failure");
            break;
        }

        for (int i = 0; i < number; i++)
        {
            int sockfd = events[i].data.fd;

            if (sockfd == listenfd)
            {
                struct sockaddr_in client_address;
                socklen_t client_addrlength = sizeof(client_address);
#ifdef listenfdLT
                int connfd = accept(listenfd, (struct sockaddr*)&client_address, &client_addrlength);
                if (connfd < 0)
                {
                    LOG_ERROR("%s:errno is:%d", "accept error", errno);
                    continue;
                }
                if (http_conn::m_user_count >= MAX_FD)
                {
                    show_error(connfd, "Internal server busy");
                    LOG_ERROR("%s", "Internal server busy");
                    continue;
                }
                users[connfd].init(connfd, client_address);

                users_timer[connfd].address = client_address;
                users_timer[connfd].sockfd = connfd;
                util_timer* timer = new util_timer;
                timer->user_data = &users_timer[connfd];
                timer->cb_func = cb_func;
                time_t cur = time(NULL);
                timer->expire = cur + TIMESLOT_TIMES * TIMESLOT;
                users_timer[connfd].timer = timer;
                timer_lst.add_timer(timer);
#endif

#ifdef listenfdET
                while (1)
                {
                    int connfd = accept(listenfd, (struct sockaddr*)&client_address, &client_addrlength);
                    if (connfd < 0)
                    {
                        LOG_ERROR("%s:errno is:%d", "accept error", errno);
                        break;
                    }
                    if (http_conn::m_user_count >= MAX_FD)
                    {
                        show_error(connfd, "Internal server busy");
                        LOG_ERROR("%s", "Internal server busy");
                        break;
                    }
                    users[connfd].init(connfd, client_address);

                    users_timer[connfd].address = client_address;
                    users_timer[connfd].sockfd = connfd;
                    util_timer* timer = new util_timer;
                    timer->user_data = &users_timer[connfd];
                    timer->cb_func = cb_func;
                    time_t cur = time(NULL);
                    timer->expire = cur + TIMESLOT_TIMES * TIMESLOT;
                    users_timer[connfd].timer = timer;
                    timer_lst.add_timer(timer);
                }
                continue;
#endif
            }

            else if (events[i].events & (EPOLLRDHUP | EPOLLHUP | EPOLLERR))
            {
                util_timer* timer = users_timer[sockfd].timer;
                timer->cb_func(&users_timer[sockfd]);

                if (timer)
                {
                    timer_lst.del_timer(timer);
                }
            }

            else if ((sockfd == pipefd[0]) && (events[i].events & EPOLLIN))
            {
                int sig;
                char signals[1024];
                ret = recv(pipefd[0], signals, sizeof(signals), 0);
                if (-1 == ret)
                {
                    continue;
                }
                else if (0 == ret)
                {
                    continue;
                }
                else
                {
                    for (int j = 0; j < ret; ++j)
                    {
                        switch (signals[j])
                        {
                        	case SIGALRM:
                        	{
                            	timeout = true;
                            	break;
                        	}
                        	case SIGTERM:
                        	{
                            	stop_server = true;
                        	}
                        }
                    }
                }
            }

            else if (events[i].events & EPOLLIN)
            {
                util_timer* timer = users_timer[sockfd].timer;
                if (users[sockfd].read_once())
                {
                    LOG_INFO("deal with the client(%s)", inet_ntoa(users[sockfd].get_address()->sin_addr));
                    Log::get_instance()->flush();

                    pool->append(users + sockfd);

                    if (timer)
                    {
                        time_t cur = time(NULL);
                        timer->expire = cur + TIMESLOT_TIMES * TIMESLOT;
                        LOG_INFO("%s", "adjust timer once");
                        Log::get_instance()->flush();
                        timer_lst.adjust_timer(timer);
                    }
                }
                else
                {
                    timer->cb_func(&users_timer[sockfd]);
                    if (timer)
                    {
                        timer_lst.del_timer(timer);
                    }
                }
            }
            else if (events[i].events & EPOLLOUT)
            {
                util_timer* timer = users_timer[sockfd].timer;
                if (users[sockfd].write())
                {
                    LOG_INFO("send data to the client(%s)", inet_ntoa(users[sockfd].get_address()->sin_addr));
                    Log::get_instance()->flush();

                    if (timer)
                    {
                        time_t cur = time(NULL);
                        timer->expire = cur + TIMESLOT_TIMES * TIMESLOT;
                        LOG_INFO("%s", "adjust timer once");
                        Log::get_instance()->flush();
                        timer_lst.adjust_timer(timer);
                    }
                }
                else
                {
                    timer->cb_func(&users_timer[sockfd]);
                    if (timer)
                    {
                        timer_lst.del_timer(timer);
                    }
                }
            }
        }
        if (timeout)
        {
            timer_handler();
            timeout = false;
        }
    }
    close(epollfd);
    close(listenfd);
    close(pipefd[1]);
    close(pipefd[0]);
    delete[] users;
    delete[] users_timer;
    delete pool;
    return 0;
}
