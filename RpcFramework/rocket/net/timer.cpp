#include <sys/timerfd.h>
#include <cstring>
#include "rocket/net/timer.h"
#include "rocket/common/log.h"
#include "rocket/common/util.h"

namespace rocket {

    Timer::Timer() : FdEvent() {

        m_fd = timerfd_create(CLOCK_MONOTONIC, TFD_NONBLOCK | TFD_CLOEXEC);
        DEBUGLOG("timer fd=%d", m_fd);

        // 将fd可读事件，挂到eventloop上监听
        listen(FdEvent::IN_EVENT, std::bind(&Timer::onTimer, this));
    }

    Timer::~Timer() {

    }

    void Timer::onTimer() {
        // 处理缓冲区数据，防止下一次触发可读时间
        char buf[8];
        while (1) {
            if ((read(m_fd, buf, 8) == -1) && errno == EAGAIN) {
                break;
            }
        }

        // 执行定时任务
        int64_t now = getNowMs();

        std::vector<TimerEvent::s_ptr> temp;
        std::vector<std::pair<int64_t, std::function<void()>>> tasks;
        ScopeMutex<Mutex> lock(m_mutex);
        auto it = m_pending_events.begin();

        for (it = m_pending_events.begin(); it != m_pending_events.end(); ++ it) {
            // 如果定时任务超时且未被取消，执行
            // 取出定时任务队列中指定时间在now之前的所有任务，并执行
            if ((*it).first <= now) {
                if (!(*it).second->isCanceled()) {
                    temp.push_back((*it).second);
                    tasks.push_back(std::make_pair((*it).second->getArriveTime(), (*it).second->getCallBack()));
                }
            } else {
                break; // 后面的都不用管，因为是时间有序的
            }
        }
        m_pending_events.erase(m_pending_events.begin(), it);
        lock.unlock();

        // 把需要重复的event 再次添加进去
        // tips: 为什么要删除再添加，因为map根据arriveTime自动排序，不重新添加无法排序，
        // 也会影响到其他函数，如Timer::resetArriveTime()里面的auto it = temp.begin();部分
        for (auto i = temp.begin(); i != temp.end(); i ++) {
            if ((*i)->isRepeated()) {
                // 重新调整该事件的arriveTime
                (*i)->resetArriveTime();
                addTimerEvent(*i);
            }
        }

        resetArriveTime();

        for (auto i : tasks) {
            if (i.second) {
                i.second();
            }
        }
    }

    void Timer::resetArriveTime() {
        ScopeMutex<Mutex> lock(m_mutex);
        auto temp = m_pending_events;
        lock.unlock();

        if (temp.size() == 0) {
            return;
        }
        int64_t now = getNowMs();

        auto it = temp.begin();
        int64_t interval = 0;
        if (it->second->getArriveTime() > now) {
            interval = it->second->getArriveTime() - now;
        } else {
            interval = 100; // 100ms
        }

        timespec ts;
        memset(&ts, 0, sizeof(ts));
        ts.tv_sec = interval / 1000;
        ts.tv_nsec = (interval % 1000) * 1000000;

        itimerspec value;
        memset(&value, 0, sizeof(value));
        value.it_value = ts;

        // 设置超时时间，若到达超时时间则timefd可读，读出来是超时次数
        int rt = timerfd_settime(m_fd, 0, &value, nullptr);
        if (rt != 0) {
            ERRORLOG("timerfd_settime error, errno=%d, error=%s", errno, strerror(errno));
        }
        DEBUGLOG("timer reset to %lld", now + interval);
    }

    void Timer::addTimerEvent(TimerEvent::s_ptr event) {
        bool is_reset_timerfd = false;

        ScopeMutex<Mutex> lock(m_mutex);
        // 如果队列没有定时任务，则肯定需要重设超时时间
        if (m_pending_events.empty()) {
            is_reset_timerfd = true;
        } else {
            auto it = m_pending_events.begin();
            // 当前要插入的定时任务，其指定时间比队列的任务指定时间都要早，则需要重设超时时间
            if ((*it).second->getArriveTime() > event->getArriveTime()) {
                is_reset_timerfd = true;
            }
        }
        m_pending_events.emplace(event->getArriveTime(), event); // 容器会自动排序
        lock.unlock();

        if (is_reset_timerfd) {
            resetArriveTime();
        }
    }

    void Timer::deleteTimerEvent(TimerEvent::s_ptr event) {
        event->setCancled(true);

        ScopeMutex<Mutex> lock(m_mutex);
        // 找到一个集合，从集合内找event
        auto begin = m_pending_events.lower_bound(event->getArriveTime()); // 队列中第一个arrivetime等于event->getArriveTime()
        auto end = m_pending_events.upper_bound(event->getArriveTime()); // 队列中最后一个arrivetime等于event->getArriveTime()

        auto it = begin;
        for (it = begin; it != end; ++ it) {
            if (it->second == event) {
                break;
            }
        }
        if (it != end) {
            m_pending_events.erase(it);
        }
        lock.unlock();
        DEBUGLOG("success delete TimerEvent at arrive time %lld", event->getArriveTime());
    }

}