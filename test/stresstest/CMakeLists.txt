# Add the unit test executable
add_executable(anudbstresstests AnuDBStressTest.cpp)

# Link against your project library and GTest
target_link_libraries(anudbstresstests PRIVATE
    libanu
    gtest_main
)

# Add include directories
target_include_directories(anudbstresstests PRIVATE
    ${CMAKE_SOURCE_DIR}/src ${CMAKE_SOURCE_DIR}/third_party/json
)

# Discover tests
include(GoogleTest)
gtest_discover_tests(anudbstresstests)
