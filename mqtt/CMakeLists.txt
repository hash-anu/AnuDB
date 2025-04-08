add_executable(MqttBridge AnuDBMqttClient.cpp)

add_subdirectory(${CMAKE_SOURCE_DIR}/third_party/nanomq "${CMAKE_BINARY_DIR}/nanomq-build" )

# Link against your project library
target_link_libraries(MqttBridge PRIVATE
    libanu
    nng
)

# Add include directories
target_include_directories(MqttBridge PRIVATE
    ${CMAKE_SOURCE_DIR}/src ${CMAKE_SOURCE_DIR}/third_party/json ${CMAKE_SOURCE_DIR}/third_party/nanomq/nanomq/include ${CMAKE_SOURCE_DIR}/nng/include/nng/
)
