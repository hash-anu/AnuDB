add_executable(AnuDBMqttBridge AnuDBMqttClient.cpp)

# Enable TLS for NanoMQ
# Add MbedTLS first so it's available to NanoMQ
set(NNG_ENABLE_TLS ON CACHE BOOL "Enable TLS in NanoMQ" FORCE)
set(ENABLE_TESTING ON CACHE BOOL "Disable testing of mbedtls and nanomq" FORCE)
add_subdirectory(${CMAKE_SOURCE_DIR}/third_party/nanomq/extern/l8w8jwt/lib/mbedtls "${CMAKE_BINARY_DIR}/mbedtls-build")

add_subdirectory(${CMAKE_SOURCE_DIR}/third_party/nanomq "${CMAKE_BINARY_DIR}/nanomq-build" )

# Link against your project library
target_link_libraries(AnuDBMqttBridge PRIVATE
    libanu
    nng
)

# Add include directories
target_include_directories(AnuDBMqttBridge PRIVATE
    ${CMAKE_SOURCE_DIR}/src ${CMAKE_SOURCE_DIR}/third_party/json ${CMAKE_SOURCE_DIR}/third_party/nanomq/nanomq/include ${CMAKE_SOURCE_DIR}/nng/include/nng/
)
