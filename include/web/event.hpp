#ifndef _RWG_WEB_EVENT_
#define _RWG_WEB_EVENT_

#include <sys/epoll.h>

namespace rwg_web {

class event {
    ::epoll_event _event;
public:
    event();
    virtual ~event();
     
    ::epoll_event& ep_event();
};

}

#endif
