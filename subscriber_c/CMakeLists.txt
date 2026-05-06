cmake_minimum_required(VERSION 3.10)
project(pubsub_subscriber C)

set(CMAKE_C_STANDARD 99)

set(OPEN62541_ROOT "C:/Users/ines-user/projects/PUBSUB_C/open62541")
set(MBEDTLS_ROOT "C:/Users/ines-user/projects/PUBSUB_C/mbedtls_v2")

include_directories(
    ${OPEN62541_ROOT}/include
    ${OPEN62541_ROOT}/build
    ${OPEN62541_ROOT}/build/src_generated
    ${OPEN62541_ROOT}/plugins/include
    ${OPEN62541_ROOT}/arch
    ${MBEDTLS_ROOT}/include
)

add_executable(subscriber main.c)

target_link_libraries(subscriber
    ${OPEN62541_ROOT}/build/bin/open62541.lib
    ${MBEDTLS_ROOT}/build/library/mbedtls.lib
    ${MBEDTLS_ROOT}/build/library/mbedx509.lib
    ${MBEDTLS_ROOT}/build/library/mbedcrypto.lib
    ws2_32
    iphlpapi
    bcrypt
)