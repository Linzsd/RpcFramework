cmake_minimum_required(VERSION 3.28)
project(rocket)

SET(CMAKE_INSTALL_PREFIX ${PROJECT_SOURCE_DIR})
enable_language(ASM)

SET(CMAKE_CXX_STANDARD 17)

set(PATH_LIB lib)
set(PATH_BIN bin)
set(PATH_TESTCASES testcases)

include_directories(${PROJECT_SOURCE_DIR})
include_directories("/usr/include/tinyxml")

aux_source_directory(${PROJECT_SOURCE_DIR}/rocket/common COMMON)
aux_source_directory(${PROJECT_SOURCE_DIR}/rocket/coroutine COR)
aux_source_directory(${PROJECT_SOURCE_DIR}/rocket/net NET)
aux_source_directory(${PROJECT_SOURCE_DIR}/rocket/net/tcp TCP)
aux_source_directory(${PROJECT_SOURCE_DIR}/rocket/net/coder CODER)
aux_source_directory(${PROJECT_SOURCE_DIR}/rocket/net/rpc RPC)

set(COXTX ${PROJECT_SOURCE_DIR}/rocket/coroutine/coctx_swap.S)

add_library(rocket ${COMMON} ${COR} ${NET} ${TCP} ${CODER} ${RPC} ${COXTX})
install(TARGETS rocket DESTINATION ${PATH_LIB})

find_package(Protobuf REQUIRED)
if (${PROTOBUF_FOUND})
    message("protobuf found")
else ()
    message(FATAL_ERROR "Cannot find Protobuf")
endif ()

set(tinyxml /usr/lib/libtinyxml.a)

set(LIBS
    rocket
    ${Protobuf_LIBRARY}
    ${tinyxml}
    dl
    pthread)

# test log
add_executable(test_log ${PROJECT_SOURCE_DIR}/${PATH_TESTCASES}/test_log.cpp)
target_link_libraries(test_log ${LIBS})
install(TARGETS test_log DESTINATION ${PATH_BIN})

# test log
add_executable(test_coroutine ${PROJECT_SOURCE_DIR}/${PATH_TESTCASES}/test_coroutine.cpp)
target_link_libraries(test_coroutine ${LIBS})
install(TARGETS test_coroutine DESTINATION ${PATH_BIN})

# test eventloop
add_executable(test_eventloop ${PROJECT_SOURCE_DIR}/${PATH_TESTCASES}/test_eventloop.cpp)
target_link_libraries(test_eventloop ${LIBS})
install(TARGETS test_eventloop DESTINATION ${PATH_BIN})

# test tcp
add_executable(test_tcp ${PROJECT_SOURCE_DIR}/${PATH_TESTCASES}/test_tcp.cpp)
target_link_libraries(test_tcp ${LIBS})
install(TARGETS test_tcp DESTINATION ${PATH_BIN})

# test client
add_executable(test_client ${PROJECT_SOURCE_DIR}/${PATH_TESTCASES}/test_client.cpp)
target_link_libraries(test_client ${LIBS})
install(TARGETS test_client DESTINATION ${PATH_BIN})

# test rpc server
add_executable(test_rpc_server ${PROJECT_SOURCE_DIR}/${PATH_TESTCASES}/test_rpc_server.cpp
                                ${PROJECT_SOURCE_DIR}/${PATH_TESTCASES}/order.pb.cc)
target_link_libraries(test_rpc_server ${LIBS})
install(TARGETS test_rpc_server DESTINATION ${PATH_BIN})

# test rpc client
add_executable(test_rpc_client ${PROJECT_SOURCE_DIR}/${PATH_TESTCASES}/test_rpc_client.cpp
                                ${PROJECT_SOURCE_DIR}/${PATH_TESTCASES}/order.pb.cc)
target_link_libraries(test_rpc_client ${LIBS})
install(TARGETS test_rpc_client DESTINATION ${PATH_BIN})

# test rpc register
add_executable(test_rpc_register ${PROJECT_SOURCE_DIR}/${PATH_TESTCASES}/test_rpc_register.cpp)
target_link_libraries(test_rpc_register ${LIBS})
install(TARGETS test_rpc_register DESTINATION ${PATH_BIN})