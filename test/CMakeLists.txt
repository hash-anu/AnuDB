# Find or fetch GTest once for all test directories
include(FetchContent)
FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG release-1.12.1  # or another stable version
)
# For Windows: Prevent overriding the parent project's compiler/linker settings
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

# Download any other test dependencies here

# Add test subdirectories
add_subdirectory(unittest)

# Conditionally include stress tests
option(BUILD_STRESS_TESTS "Build stress tests" ON)
if(BUILD_STRESS_TESTS)
    add_subdirectory(stresstest)
endif()
