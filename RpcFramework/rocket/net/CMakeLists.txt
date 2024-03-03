set(HEADERS
        eventloop.h
        fd_event.h
        timer_event.h
        timer.h
        io_thread.h
        io_thread_group.h
)

install(FILES ${HEADERS} DESTINATION include/rocket/net)