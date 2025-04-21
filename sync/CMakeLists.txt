add_executable(AnuDBSyncServer AnuDBSyncServer.cpp)

# Link against your project library
target_link_libraries(AnuDBSyncServer PRIVATE
    libanu
)

# Add include directories
target_include_directories(AnuDBSyncServer PRIVATE
    ${CMAKE_SOURCE_DIR}/src ${CMAKE_SOURCE_DIR}/third_party/json ${CMAKE_SOURCE_DIR}/third_party/nanomq/nanomq/include ${CMAKE_SOURCE_DIR}/nng/include/nng/
)
