#include "rocket/net/fd_event_group.h"
#include "rocket/common/mutex.h"
#include "rocket/common/log.h"

namespace rocket {

    static FdEventGroup* g_fd_event_group = nullptr;

    FdEventGroup *FdEventGroup::GetFdEventGroup() {
        if (g_fd_event_group != nullptr) {
            return g_fd_event_group;
        }
        g_fd_event_group = new FdEventGroup(128);
        return g_fd_event_group;
    }

    FdEventGroup::FdEventGroup(int size) : m_size(size) {
        for (int i = 0; i < m_size; ++i) {
            m_fd_group.push_back(new FdEvent(i));
        }
    }

    FdEventGroup::~FdEventGroup() {
        for (int i = 0; i < m_size; ++i) {
            if (m_fd_group[i] != nullptr) {
                delete m_fd_group[i];
                m_fd_group[i] = nullptr;
            }
        }
    }

    FdEvent *FdEventGroup::getFdEvent(int fd) {
        // 在栈上创建锁，超出作用域自动销毁≈解锁，因此不需要unlock
        ScopeMutex<Mutex> lock(m_mutex);
        if (fd < m_fd_group.size()) {
            return m_fd_group[fd];
        }
        // 扩容
        int new_size = int(fd * 1.5);
        for (int i = m_fd_group.size(); i < new_size; i ++) {
            m_fd_group.push_back(new FdEvent(i));
        }
        return m_fd_group[fd];
    }

}
