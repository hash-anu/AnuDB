cmake_minimum_required(VERSION 3.14)
project(AnuDB VERSION 1.0.0 LANGUAGES CXX)

# Use C++11 standard
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Set build type to Release if not specified
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

if (CMAKE_SYSTEM_NAME STREQUAL "Linux")
    add_compile_definitions(MAKE_UNIQUE)
    add_compile_options(-Wno-error=array-bounds)
    #add_compile_options(-std=c++11)  # For GCC/Clang
endif()

#test dir
# Testing setup - make it optional with a CMake option
option(BUILD_TESTS "Build tests" ON)

if(BUILD_TESTS)
    enable_testing()
    add_subdirectory(test)
endif()

# 🔹 Allow passing Zstd paths from the command line
find_path(ZSTD_INCLUDE_DIR NAMES zstd.h)
find_library(ZSTD_LIBRARY NAMES zstd_static)

# Ensure Zstd paths are found
if(ZSTD_INCLUDE_DIR AND ZSTD_LIBRARY)
	add_compile_definitions(ZSTD)
	# Enable Zstd in RocksDB
	set(WITH_ZSTD ON CACHE BOOL "Enable Zstd compression in RocksDB")
	include_directories(${ZSTD_INCLUDE_DIR})
	link_directories(${ZSTD_LIBRARY})
endif()


include_directories(src/)


include_directories(third_party/json)
include_directories(src/storage_engine)
include_directories(${CMAKE_SOURCE_DIR}/third_party/rocksdb/include)

set(WITH_TESTS OFF CACHE BOOL "Disable tests" FORCE)
set(WITH_GFLAGS OFF CACHE BOOL "Disable GFlags" FORCE)
set(WITH_BENCHMARK_TOOLS OFF CACHE BOOL "Disable benchmark tools" FORCE)
set(WITH_CORE_TOOLS OFF CACHE BOOL "Disable core tools" FORCE)
set(WITH_TOOLS OFF CACHE BOOL "Disable tools" FORCE)
set(WITH_FUZZER OFF CACHE BOOL "Disable fuzzing tools" FORCE)

# Add RocksDB as a subdirectory
add_subdirectory(third_party/rocksdb)
# Add storage engine
add_subdirectory(src/storage_engine)

set(LIBRARY_SOURCES ${CMAKE_SOURCE_DIR}/src/Cursor.cpp ${CMAKE_SOURCE_DIR}/src/Database.cpp ${CMAKE_SOURCE_DIR}/src/Collection.cpp ${CMAKE_SOURCE_DIR}/src/Document.cpp)

add_library(libanu STATIC ${LIBRARY_SOURCES})

target_link_libraries(libanu rocksdb libstorage)

if(ZSTD_INCLUDE_DIR AND ZSTD_LIBRARY)
	target_link_libraries(libanu ${ZSTD_LIBRARY})
endif()

# On Unix-like systems, link pthread if needed
if(UNIX)
    target_link_libraries(libanu pthread)
endif()
# Create the main executable
add_executable(AnuDB ${CMAKE_SOURCE_DIR}/src/main.cpp)

# Link RocksDB and Storage Engine
target_link_libraries(AnuDB libanu)

add_subdirectory(examples)


# Force static library build for resource efficiency
set(BUILD_SHARED_LIBS OFF CACHE BOOL "Build shared libraries" FORCE)

# Essential security options
set(ENABLE_TLS ON CACHE BOOL "Enable TLS support" FORCE)
set(ENABLE_ACL ON CACHE BOOL "Enable basic access control" FORCE)

# Disable features that consume excessive resources
set(ENABLE_RULE_ENGINE OFF CACHE BOOL "Disable rule engine to save resources" FORCE)
set(ENABLE_PROMETHEUS OFF CACHE BOOL "Disable metrics for resource saving" FORCE)
set(ENABLE_ACME OFF CACHE BOOL "Disable ACME protocol" FORCE)

# Use lightweight TLS if available
set(ENABLE_MBEDTLS ON CACHE BOOL "Use mbedTLS instead of OpenSSL" FORCE)
set(ENABLE_OPENSSL OFF CACHE BOOL "Disable OpenSSL" FORCE)
include_directories(${CMAKE_SOURCE_DIR}/third_party/nanomq/nanomq/include)
# Add mqtt file
add_subdirectory(mqtt)
