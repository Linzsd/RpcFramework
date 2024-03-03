set(HEADERS
        abstract_coder.h
        abstract_protocol.h
        tinypb_coder.h
        tinypb_protocol.h
)

install(FILES ${HEADERS} DESTINATION include/rocket/net/coder)