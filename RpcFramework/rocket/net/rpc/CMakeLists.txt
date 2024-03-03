set(HEADERS
        rpc_dispatcher.h
        rpc_controller.h
        rpc_closure.h
        rpc_channel.h
        rpc_registry.h
        register_server.pb.h
)

install(FILES ${HEADERS} DESTINATION include/rocket/net/rpc)