set(storage_engine_SRCS
    StorageEngine.h
    StorageEngine.cpp
    ${CMAKE_SOURCE_DIR}/third_party/rocksdb/include
)


# Declare the library
add_library(libstorage STATIC
    ${storage_engine_SRCS}
)

target_include_directories(libstorage PUBLIC ${CMAKE_SOURCE_DIR}/third_party/rocksdb/include)

# Specify here the include directories exported
# by this library
target_include_directories(libstorage PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)

