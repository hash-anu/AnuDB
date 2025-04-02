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

add_executable(ExportThenImportCollection AnuDBExportThenImport.cpp)

# Link against your project library 
target_link_libraries(ExportThenImportCollection PRIVATE
    libanu
)

# Add include directories
target_include_directories(ExportThenImportCollection PRIVATE
    ${CMAKE_SOURCE_DIR}/src ${CMAKE_SOURCE_DIR}/third_party/json
)

add_executable(WriteSetUpdateReadDocument AnuDBWriteSetUpdateReadDocument.cpp)

# Link against your project library 
target_link_libraries(WriteSetUpdateReadDocument PRIVATE
    libanu
)

# Add include directories
target_include_directories(WriteSetUpdateReadDocument PRIVATE
    ${CMAKE_SOURCE_DIR}/src ${CMAKE_SOURCE_DIR}/third_party/json
)

add_executable(WriteUnsetUpdateReadDocument AnuDBWriteUnSetUpdateReadDocument.cpp)

# Link against your project library 
target_link_libraries(WriteUnsetUpdateReadDocument PRIVATE
    libanu
)

# Add include directories
target_include_directories(WriteUnsetUpdateReadDocument PRIVATE
    ${CMAKE_SOURCE_DIR}/src ${CMAKE_SOURCE_DIR}/third_party/json
)

add_executable(WritePushUpdateReadDocument AnuDBWritePushUpdateReadDocument.cpp)

# Link against your project library 
target_link_libraries(WritePushUpdateReadDocument PRIVATE
    libanu
)

# Add include directories
target_include_directories(WritePushUpdateReadDocument PRIVATE
    ${CMAKE_SOURCE_DIR}/src ${CMAKE_SOURCE_DIR}/third_party/json
)

add_executable(WritePopUpdateReadDocument AnuDBWritePopUpdateReadDocument.cpp)

# Link against your project library 
target_link_libraries(WritePopUpdateReadDocument PRIVATE
    libanu
)

# Add include directories
target_include_directories(WritePopUpdateReadDocument PRIVATE
    ${CMAKE_SOURCE_DIR}/src ${CMAKE_SOURCE_DIR}/third_party/json
)
