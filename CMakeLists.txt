#
#
#

if(NOT SF_MSGBUS_BLACKBOX2_STANDALONE)
    message(STATUS "sf-msgbus enable blackbox2.")
    target_sources(${LIBRARY_NAME}_objs
        PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/channel_stub_impl.cpp
            ${CMAKE_CURRENT_SOURCE_DIR}/client.cpp
            ${CMAKE_CURRENT_SOURCE_DIR}/context.cpp
            ${CMAKE_CURRENT_SOURCE_DIR}/executor_stub_impl.cpp
            ${CMAKE_CURRENT_SOURCE_DIR}/handle_stub_impl.cpp
            ${CMAKE_CURRENT_SOURCE_DIR}/local_player.cpp
            ${CMAKE_CURRENT_SOURCE_DIR}/local_recorder.cpp
            ${CMAKE_CURRENT_SOURCE_DIR}/node_stub_impl.cpp
            ${CMAKE_CURRENT_SOURCE_DIR}/protocol.cpp
            ${CMAKE_CURRENT_SOURCE_DIR}/protocol_message.pb.cc
        )
    if(NOT SF_MSGBUS_BLACKBOX)
        target_sources(${LIBRARY_NAME}_objs PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/enet.cpp)
    endif()
    if(WIN32)
        target_sources(${LIBRARY_NAME}_objs PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/pipe_win32.cpp)
    else()
        target_sources(${LIBRARY_NAME}_objs PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/pipe_posix.cpp)
    endif()
endif()

include(FindProtobuf)
protobuf_generate_cpp(PROTOCOL_MESSAGE_SRCS PROTOCOL_MESSAGE_HDRS protocol_message.proto)

set(SF_MSGBUS_BLACKBOX2_SERVER_SOURCES
    channel_proxy_impl.cpp
    context.cpp
    enet.cpp
    executor_proxy_impl.cpp
    handle_proxy_impl.cpp
    local_player.cpp
    local_recorder.cpp
    node_proxy_impl.cpp
    process_proxy_impl.cpp
    protocol.cpp
    server.cpp
    ${PROTOCOL_MESSAGE_SRCS}
    )

if(WIN32)
    list(APPEND SF_MSGBUS_BLACKBOX2_SERVER_SOURCES pipe_win32.cpp)
else()
    list(APPEND SF_MSGBUS_BLACKBOX2_SERVER_SOURCES pipe_posix.cpp)
endif()

find_package(protobuf 3.19.3 REQUIRED)

add_library(sf-msgbus-blackbox2-shared SHARED ${SF_MSGBUS_BLACKBOX2_SERVER_SOURCES})
set_target_properties(sf-msgbus-blackbox2-shared PROPERTIES OUTPUT_NAME "sf-msgbus-blackbox2")
target_compile_definitions(sf-msgbus-blackbox2-shared PUBLIC SF_MSGBUS_BLACKBOX2_SERVER=1)
target_link_libraries(sf-msgbus-blackbox2-shared protobuf::libprotobuf)

add_library(sf-msgbus-blackbox2-static STATIC ${SF_MSGBUS_BLACKBOX2_SERVER_SOURCES})
set_target_properties(sf-msgbus-blackbox2-static PROPERTIES OUTPUT_NAME "sf-msgbus-blackbox2")
target_compile_definitions(sf-msgbus-blackbox2-static PUBLIC SF_MSGBUS_BLACKBOX2_SERVER=1)
target_link_libraries(sf-msgbus-blackbox2-static protobuf::libprotobuf)

install(TARGETS
            sf-msgbus-blackbox2-shared
            sf-msgbus-blackbox2-static
        EXPORT
            sf-msgbus-blackbox2-targets
        ARCHIVE DESTINATION
            ${CMAKE_INSTALL_LIBDIR}
        LIBRARY DESTINATION
            ${CMAKE_INSTALL_LIBDIR}
        RUNTIME DESTINATION
            ${CMAKE_INSTALL_BINDIR}
        )

install(EXPORT
            sf-msgbus-blackbox2-targets
        DESTINATION
            ${CMAKE_INSTALL_LIBDIR}/cmake/sf-msgbus-blackbox2
        )

set(SF_MSGBUS_INCLUDEDIR "${CMAKE_SOURCE_DIR}/asb/include/sf-msgbus")

install(FILES
            ${SF_MSGBUS_INCLUDEDIR}/types/array.h
            ${SF_MSGBUS_INCLUDEDIR}/types/result.h
            ${SF_MSGBUS_INCLUDEDIR}/types/serialize_type.h
            ${SF_MSGBUS_INCLUDEDIR}/types/sigslot.h
        DESTINATION
            ${CMAKE_INSTALL_INCLUDEDIR}/sf-msgbus/types
        )

install(FILES
            ${SF_MSGBUS_INCLUDEDIR}/log/log.h
        DESTINATION
            ${CMAKE_INSTALL_INCLUDEDIR}/sf-msgbus/log
        )

install(FILES
            ${SF_MSGBUS_INCLUDEDIR}/blackbox2/channel_proxy.h
            ${SF_MSGBUS_INCLUDEDIR}/blackbox2/common.h
            ${SF_MSGBUS_INCLUDEDIR}/blackbox2/executor_proxy.h
            ${SF_MSGBUS_INCLUDEDIR}/blackbox2/handle_proxy.h
            ${SF_MSGBUS_INCLUDEDIR}/blackbox2/message.h
            ${SF_MSGBUS_INCLUDEDIR}/blackbox2/node_proxy.h
            ${SF_MSGBUS_INCLUDEDIR}/blackbox2/proxy.h
            ${SF_MSGBUS_INCLUDEDIR}/blackbox2/server.h
        DESTINATION
            ${CMAKE_INSTALL_INCLUDEDIR}/sf-msgbus/blackbox2
        )

install(FILES
            ${CMAKE_BINARY_DIR}/asb/include/sf-msgbus/config.h
        DESTINATION
            ${CMAKE_INSTALL_INCLUDEDIR}/sf-msgbus
        )
