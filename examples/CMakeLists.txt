add_executable(WriteReadDocument AnuDBWriteReadDocument.cpp)

# Link against your project library
target_link_libraries(WriteReadDocument PRIVATE
    libanu
)

# Add include directories
target_include_directories(WriteReadDocument PRIVATE
    ${CMAKE_SOURCE_DIR}/src ${CMAKE_SOURCE_DIR}/third_party/json
)

add_executable(WriteReadUpdateWriteReadDocument AnuDBWriteReadUpdateWriteReadDocument.cpp)

# Link against your project library 
target_link_libraries(WriteReadUpdateWriteReadDocument PRIVATE
    libanu
)

# Add include directories
target_include_directories(WriteReadUpdateWriteReadDocument PRIVATE
    ${CMAKE_SOURCE_DIR}/src ${CMAKE_SOURCE_DIR}/third_party/json
)

add_executable(listCollections AnuDBCreateGetDropCollections.cpp)

# Link against your project library 
target_link_libraries(listCollections PRIVATE
    libanu
)

# Add include directories
target_include_directories(listCollections PRIVATE
    ${CMAKE_SOURCE_DIR}/src ${CMAKE_SOURCE_DIR}/third_party/json
)
