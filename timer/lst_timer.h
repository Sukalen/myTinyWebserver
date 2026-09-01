#ifndef LST_TIMER
#define LST_TIMER

#include<arpa/inet.h>
#include<chrono>
#include<list>
#include<algorithm>

#include "../log/log.h"

class http_conn;
class util_timer;

struct client_data
{
	struct sockaddr_in address{};
	int sockfd = -1;

	http_conn* conn = nullptr;
	util_timer* timer = nullptr;
};

class util_timer
{
public:
	using Clock = std::chrono::steady_clock;
	using TimePoint = Clock::time_point;
	using Callback = void (*)(client_data*);

public:
	TimePoint expire{};
	
	Callback cb_func = nullptr;

	client_data* user_data = nullptr;
};


class sort_timer_lst
{
public:
	sort_timer_lst() = default;
	~sort_timer_lst() = default;

	util_timer* add_timer(client_data* user_data, util_timer::Callback cb_func, std::chrono::seconds timeout)
	{
		util_timer timer;

		timer.expire = util_timer::Clock::now() + timeout;
		timer.cb_func = cb_func;
		timer.user_data = user_data;

		auto pos = std::find_if(m_timers.begin(), m_timers.end(),
				[&timer](const util_timer& current)
				{
					return timer.expire < current.expire;
				});

		auto it = m_timers.insert(pos,std::move(timer));

		util_timer* timer_ptr = &(*it);

		if(user_data)
		{
			user_data->timer = timer_ptr;
		}


		return timer_ptr;

	}

	void adjust_timer(util_timer* timer, std::chrono::seconds timeout)
	{
		if(!timer)
		{
			return;
		}

		auto it = find_timer(timer);

		if( it == m_timers.end())
		{
			return;
		}
		
		it->expire = util_timer::Clock::now() + timeout;
		
		m_timers.splice(m_timers.end(), m_timers, it);

		auto moved = std::prev(m_timers.end());

		auto pos = std::find_if(m_timers.begin(), moved,
				[moved](const util_timer& current)
				{
					return moved->expire < current.expire;
				});

		m_timers.splice(pos, m_timers, moved);

	}

	void del_timer(util_timer* timer)
	{
		if(!timer)
		{
			return;
		}

		auto it = find_timer(timer);

		if( it == m_timers.end())
		{
			return;
		}


		if(it->user_data && it->user_data->timer == timer)
		{
			it->user_data->timer = nullptr;
		}

		m_timers.erase(it);


	}

	void tick()
	{
		if(m_timers.empty())
		{
			return;
		}
		LOG_INFO("%s","timer tick");

		const auto now = util_timer::Clock::now();

		while(!m_timers.empty())
		{
			auto& timer = m_timers.front();

			if(timer.expire > now)
			{
				break;
			}

			client_data* user_data = timer.user_data;
			auto cb_func = timer.cb_func;

			if(user_data && user_data->timer == &timer)
			{
				user_data->timer = nullptr;
			}

			if(cb_func)
			{
				cb_func(user_data);
			}

			m_timers.pop_front();
		}
	}

private:

	using TimerList = std::list<util_timer>;
	TimerList::iterator find_timer(util_timer* timer)
	{
		return std::find_if(m_timers.begin(), m_timers.end(), 
				[timer](util_timer& current)
				{
					return &current == timer;
				});
	}


private:
	std::list<util_timer> m_timers;
};

#endif
