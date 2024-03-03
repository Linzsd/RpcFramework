set(HEADERS
        tcp_buffer.h
        tcp_acceptor.h
        net_addr.h
        tcp_server.h
        tcp_connection.h
        tcp_client.h
)

install(FILES ${HEADERS} DESTINATION include/rocket/net/tcp)