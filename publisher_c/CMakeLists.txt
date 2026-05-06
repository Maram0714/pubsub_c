cmake_minimum_required(VERSION 3.10)
project(pubsub_publisher C)

set(CMAKE_C_STANDARD 99)

set(OPEN62541_ROOT "C:/Users/ines-user/projects/PUBSUB_C/open62541")

include_directories(
    ${OPEN62541_ROOT}/include
    ${OPEN62541_ROOT}/build
    ${OPEN62541_ROOT}/build/src_generated
    ${OPEN62541_ROOT}/plugins/include
    ${OPEN62541_ROOT}/arch
)

add_executable(publisher main.c)

target_link_libraries(publisher
    ${OPEN62541_ROOT}/build/bin/open62541.lib
    ws2_32
    iphlpapi
)