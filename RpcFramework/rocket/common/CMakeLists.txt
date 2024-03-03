set(HEADERS
        config.h
        log.h
        util.h
        mutex.h
        msg_util.h
        run_time.h
)

install(FILES ${HEADERS} DESTINATION include/rocket/common)