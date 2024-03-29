#ifndef ROCKET_WAKEUP_FD_EVENT_H
#define ROCKET_WAKEUP_FD_EVENT_H

#include "rocket/net/fd_event.h"

namespace rocket {
    class WakeUpFdEvent : public FdEvent {
    public:
        WakeUpFdEvent(int fd);

        ~WakeUpFdEvent();

        void wakeup();

    private:

    };
}


#endif //ROCKET_WAKEUP_FD_EVENT_H
